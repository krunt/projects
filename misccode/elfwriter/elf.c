#include "elf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <myclib/all.h>


    /* relocation scheme 
     *
     * .text
     *
     * call <x> 
     *         
     * .plt
     * x:   jmp <y>
     *
     * .got.plt (y - is R_386_JUMP_SLOT in .rel.plt)
     * y: <d>
     *
     * */

#define entry(e) &(e)->entry
#define aligned_length(e) \
    (e)->align ? (((e)->size | ((e)->align - 1)) + 1) : (e)->size
#define round_up(n, e) \
    (n) |= ((e)->align - 1), (n) += 1
#define HASH_BUCKET_COUNT 11
#define SECTION_COUNT 13

typedef struct pheader_s {
    struct list_head entry;

    int type;
    int size;
    int flags;
    int align;

    Elf32_Addr vaddr;
    Elf32_Addr file_offset;

    struct list_head sections;

} pheader_t;

typedef struct section_s {
    struct list_head entry;
    char *data;
    int size;
    int type;
    int flags;
    int align;
    int entsize;

    Elf32_Addr vaddr;
    Elf32_Addr file_offset;

    int name_index;
    char name[1];
} section_t;

typedef struct symbol_s {
    struct list_head entry;
    Elf32_Addr value;
    int size;

    Elf32_Addr vaddr;

    int name_index;

    char name[1];
} symbol_t;

typedef struct dyntag_s {
    struct list_head entry;
    int type;
    Elf32_Addr value;
} dyntag_t;

typedef struct patching_entry_s {
    struct list_head entry;
    int offset;
} patching_entry_t;

typedef struct relocation_entry_s {
    struct list_head entry;
    struct hlist_node hentry;

    int plt_offset;
    int got_plt_offset;

    /* offset-list in text section */
    struct list_head patching_list; 

    char name[1];
} relocation_entry_t;

typedef struct elf_state_s {

    Elf32_Addr base_address;

    int section_header_offset;
    int program_header_offset;

    struct list_head pheaders;
    struct list_head sections;
    struct list_head symbols;

    section_t *s_interp;

    section_t *s_text;
    section_t *s_got;    /* where glob_dat stored */
    section_t *s_relplt; /* plt-entries */
    section_t *s_plt;    /* jumps into gotplt */
    section_t *s_gotplt; /* where loader patches entries */
    section_t *s_dynsym;
    section_t *s_dynstr;
    section_t *s_hash;

    section_t *s_data;
    section_t *s_bss;

    section_t *s_dynamic;

    section_t *s_shstrtab;

    pheader_t *p_phdr;
    pheader_t *p_interp;
    pheader_t *p_load1;
    pheader_t *p_load2;
    pheader_t *p_dynamic;

    struct list_head dtags[DT_NUM];

    /* relocation heads */
    struct hlist_head rheads[HASH_BUCKET_COUNT]; 
    struct list_head relocation_list;

} elf_state_t;

static unsigned int elf_hash(const char *name) {
    unsigned int h = 0, g;

    while (*name) {
        h = (h << 4) + *name++;
        if (g = h & 0xf0000000)
            h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

pheader_t *pheader_new(int type, int flags, int align) {
    pheader_t *res = malloc(sizeof(pheader_t));
    INIT_LIST_HEAD(&res->entry);
    res->type = type;
    res->size = 0;
    res->flags = flags;
    res->align = align;
    res->vaddr = 0;
    res->file_offset = 0;
    INIT_LIST_HEAD(&res->sections);
    return res;
}

section_t *section_new(const char *name, int type, int flags, 
        int align, int entsize) 
{
    section_t *res = malloc(sizeof(section_t) + strlen(name));
    INIT_LIST_HEAD(&res->entry);
    res->data = NULL;
    res->size = 0;
    res->type = type;
    res->flags = flags;
    res->align = align;
    res->entsize = entsize;
    res->vaddr = 0;
    res->file_offset = 0;
    memcpy(res->name, name, strlen(name));
    res->name[strlen(name)] = 0;
    return res;
}

void section_expand(section_t *section, int new_sz) {
    section->data = realloc(section->data, new_sz);
    section->size = new_sz;
}

char *section_add(section_t *section, int bytes) {
    int original_size = section->size;
    section_expand(section, bytes + original_size);
    return &section->data[original_size];
}

symbol_t *symbol_new(const char *name, int offset) {
    symbol_t *res = malloc(sizeof(symbol_t) + strlen(name));
    INIT_LIST_HEAD(&res->entry);
    res->value = offset;
    res->size = 0;
    res->vaddr = 0;
    memcpy(res->name, name, strlen(name));
    res->name[strlen(name)] = 0;
    return res;
}

dyntag_t *dyntag_new(int type) {
    dyntag_t *res = malloc(sizeof(dyntag_t));
    INIT_LIST_HEAD(&res->entry);
    res->type = type;
    res->value = 0;
    return res;
}

void elf_setup_state(elf_state_t *s) {
    int i;

    s->base_address = 0x8004000;
    s->section_header_offset = 0;
    s->program_header_offset = 0;

    INIT_LIST_HEAD(&s->pheaders);
    INIT_LIST_HEAD(&s->sections);
    INIT_LIST_HEAD(&s->symbols);

    s->s_interp = section_new(".interp", SHT_PROGBITS, SHF_ALLOC, 1, 0);
    s->s_text = section_new(".text", SHT_PROGBITS, 
            SHF_ALLOC | SHF_EXECINSTR, 16, 0);
    s->s_data = section_new(".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, 32, 0);
    s->s_bss = section_new(".bss", SHT_NOBITS, SHF_ALLOC | SHF_WRITE, 32, 0);
    s->s_dynamic = section_new(".dynamic", SHT_DYNAMIC, 
            SHF_ALLOC | SHF_WRITE, 4, 8);
    s->s_shstrtab = section_new(".shstrtab", SHT_STRTAB, 0, 1, 0);
    s->s_dynsym = section_new(".dynsym", SHT_DYNSYM, SHF_ALLOC, 4, 10);
    s->s_dynstr = section_new(".dynstr", SHT_STRTAB, SHF_ALLOC, 1, 0);
    s->s_hash = section_new(".hash", SHT_HASH, SHF_ALLOC, 4, 0);
    s->s_got = section_new(".got", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, 4, 0);
    s->s_relplt = section_new(".rel.plt", SHT_REL, SHF_ALLOC, 4, 8);
    s->s_plt = section_new(".plt", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, 4, 0);
    s->s_gotplt = section_new(".got.plt", SHT_PROGBITS, 
            SHF_ALLOC | SHF_WRITE, 0, 0);

    /* after changing this order - change indices in section2shdr */
    list_add_tail(entry(s->s_interp), &s->sections);  /* 1 */
    list_add_tail(entry(s->s_text), &s->sections);
    list_add_tail(entry(s->s_data), &s->sections);
    list_add_tail(entry(s->s_bss), &s->sections);
    list_add_tail(entry(s->s_dynamic), &s->sections); /* 5 */
    list_add_tail(entry(s->s_dynsym), &s->sections);
    list_add_tail(entry(s->s_dynstr), &s->sections);
    list_add_tail(entry(s->s_hash), &s->sections);
    list_add_tail(entry(s->s_got), &s->sections);
    list_add_tail(entry(s->s_relplt), &s->sections); /* 10 */
    list_add_tail(entry(s->s_plt), &s->sections);
    list_add_tail(entry(s->s_gotplt), &s->sections);
    list_add_tail(entry(s->s_shstrtab), &s->sections); /* 13 */

    s->p_phdr = pheader_new(PT_PHDR, PF_R | PF_X, 4);
    s->p_interp = pheader_new(PT_INTERP, PF_R, 1);
    s->p_load1 = pheader_new(PT_LOAD, PF_R | PF_X, 0x1000);
    s->p_load2 = pheader_new(PT_LOAD, PF_R | PF_W, 0x1000);
    s->p_dynamic = pheader_new(PT_DYNAMIC, PF_R | PF_W, 4);

    list_add_tail(entry(s->p_phdr), &s->pheaders);
    list_add_tail(entry(s->p_interp), &s->pheaders);
    list_add_tail(entry(s->p_load1), &s->pheaders);
    list_add_tail(entry(s->p_load2), &s->pheaders);
    list_add_tail(entry(s->p_dynamic), &s->pheaders);

    for (i = 0; i < DT_NUM; ++i) {
        INIT_LIST_HEAD(&s->dtags[i]);
    }

    list_add_tail(entry(dyntag_new(DT_NULL)), &s->dtags[DT_NULL]);
    list_add_tail(entry(dyntag_new(DT_HASH)), &s->dtags[DT_HASH]);
    list_add_tail(entry(dyntag_new(DT_STRTAB)), &s->dtags[DT_STRTAB]);
    list_add_tail(entry(dyntag_new(DT_SYMTAB)), &s->dtags[DT_SYMTAB]);
    list_add_tail(entry(dyntag_new(DT_REL)), &s->dtags[DT_REL]);
    list_add_tail(entry(dyntag_new(DT_RELSZ)), &s->dtags[DT_RELSZ]);
    list_add_tail(entry(dyntag_new(DT_RELENT)), &s->dtags[DT_RELENT]);
    list_add_tail(entry(dyntag_new(DT_STRSZ)), &s->dtags[DT_STRSZ]);
    list_add_tail(entry(dyntag_new(DT_SYMENT)), &s->dtags[DT_SYMENT]);

    for (i = 0; i < HASH_BUCKET_COUNT; ++i) {
        INIT_HLIST_HEAD(&s->rheads[i]);
    }

    INIT_LIST_HEAD(&s->relocation_list);
}

/* TODO */
void elf_free_state(elf_state_t *s) {
}

void setup_dynamic_section(elf_state_t *s, Elf32_Addr *vaddr, int *len) {
    int i;
    dyntag_t *t;

    for (i = 0; i < DT_NUM; ++i) {
        list_for_each_entry(t, &s->dtags[i], entry) {
            //t->value = *vaddr;
            *vaddr += sizeof(Elf32_Dyn);
            *len += sizeof(Elf32_Dyn);
        }
    }
}

void setup_relplt_section(elf_state_t *s, Elf32_Addr *vaddr, int *len) {
    relocation_entry_t *t;

    list_for_each_entry(t, &s->relocation_list, entry) {
        //t->value = *vaddr;
        *vaddr += sizeof(Elf32_Rel);
        *len += sizeof(Elf32_Rel);
    }
}

void setup_syms_section(elf_state_t *s, Elf32_Addr *vaddr, int *len) {
    symbol_t *t;

    list_for_each_entry(t, &s->symbols, entry) {
        //t->value = *vaddr;
        *vaddr += sizeof(Elf32_Sym);
        *len += sizeof(Elf32_Sym);
    }
}

void setup_phdr_va_length(elf_state_t *s, Elf32_Addr *vaddr, int *foffset) {
    s->p_phdr->vaddr = *vaddr;
    s->p_phdr->file_offset = s->program_header_offset;
    s->p_phdr->size = sizeof(Elf32_Phdr) * 5;

    *vaddr += s->p_phdr->size;

    round_up(*vaddr, s->p_phdr);
    round_up(*foffset, s->p_phdr);
}

void setup_interp_va_length(elf_state_t *s, Elf32_Addr *vaddr, int *foffset) {
    s->p_interp->vaddr = *vaddr;
    s->p_interp->file_offset = *foffset;
    s->p_interp->size = s->s_interp->size;

    s->s_interp->vaddr = *vaddr;
    s->s_interp->file_offset = *foffset;

    *vaddr += aligned_length(s->s_interp);
    *foffset += aligned_length(s->s_interp);

    round_up(*vaddr, s->p_interp);
    round_up(*foffset, s->p_interp);
}

void setup_load1_va_length(elf_state_t *s, Elf32_Addr *vaddr, int *foffset) {
    int orig_offset = *foffset;

    s->p_load1->vaddr = *vaddr;
    s->p_load1->file_offset = *foffset;

    s->s_text->vaddr = *vaddr;
    s->s_text->file_offset = *foffset;

    *vaddr += aligned_length(s->s_text);
    *foffset += aligned_length(s->s_text);

    s->s_got->vaddr = *vaddr;
    s->s_got->file_offset = *foffset;

    *vaddr += aligned_length(s->s_got);
    *foffset += aligned_length(s->s_got);

    s->s_relplt->vaddr = *vaddr;
    s->s_relplt->file_offset = *foffset;

    setup_relplt_section(s, vaddr, foffset);
    round_up(*vaddr, s->s_relplt);
    round_up(*foffset, s->s_relplt);

    s->s_plt->vaddr = *vaddr;
    s->s_plt->file_offset = *foffset;

    *vaddr += aligned_length(s->s_plt);
    *foffset += aligned_length(s->s_plt);

    s->s_gotplt->vaddr = *vaddr;
    s->s_gotplt->file_offset = *foffset;

    *vaddr += aligned_length(s->s_gotplt);
    *foffset += aligned_length(s->s_gotplt);

    s->s_dynsym->vaddr = *vaddr;
    s->s_dynsym->file_offset = *foffset;

    setup_syms_section(s, vaddr, foffset);
    round_up(*vaddr, s->s_dynsym);
    round_up(*foffset, s->s_dynsym);

    s->s_dynstr->vaddr = *vaddr;
    s->s_dynstr->file_offset = *foffset;

    *vaddr += aligned_length(s->s_dynstr);
    *foffset += aligned_length(s->s_dynstr);

    s->s_hash->vaddr = *vaddr;
    s->s_hash->file_offset = *foffset;

    *vaddr += aligned_length(s->s_hash);
    *foffset += aligned_length(s->s_hash);

    round_up(*vaddr, s->s_hash);
    round_up(*foffset, s->s_hash);

    s->p_load1->size = *foffset - orig_offset;

    round_up(*vaddr, s->p_load1);
    round_up(*foffset, s->p_load1);
}

void setup_load2_va_length(elf_state_t *s, Elf32_Addr *vaddr, int *foffset) {
    int orig_offset = *foffset;

    s->p_load2->vaddr = *vaddr;
    s->p_load2->file_offset = *foffset;

    s->s_data->vaddr = *vaddr;
    s->s_data->file_offset = *foffset;

    *vaddr += aligned_length(s->s_data);
    *foffset += aligned_length(s->s_data);

    s->s_bss->vaddr = *vaddr;
    s->s_bss->file_offset = *foffset;

    *vaddr += aligned_length(s->s_bss);
    //*foffset += aligned_length(s->s_bss);

    s->p_load2->size = *foffset - orig_offset;

    round_up(*vaddr, s->p_load2);
    round_up(*foffset, s->p_load2);
}

void setup_dynamic_va_length(elf_state_t *s, Elf32_Addr *vaddr, int *foffset) {
    int orig_offset = *foffset;

    s->p_dynamic->vaddr = *vaddr;
    s->p_dynamic->file_offset = *foffset;

    s->s_dynamic->vaddr = *vaddr;
    s->s_dynamic->file_offset = *foffset;

    setup_dynamic_section(s, vaddr, foffset);

    s->p_dynamic->size = *foffset - orig_offset;

    round_up(*vaddr, s->p_dynamic);
    round_up(*foffset, s->p_dynamic);
}


int setup_output_file_va_length(elf_state_t *s) {
    Elf32_Addr vaddr;
    int foffset;

    vaddr = s->base_address;

    foffset = 0;
    foffset += sizeof(Elf32_Ehdr);

    s->section_header_offset = foffset;
    foffset += sizeof(Elf32_Shdr) * (SECTION_COUNT + 1);

    s->program_header_offset = foffset;
    foffset += sizeof(Elf32_Phdr) * 5;

    setup_phdr_va_length(s, &vaddr, &foffset);
    setup_interp_va_length(s, &vaddr, &foffset);
    setup_load1_va_length(s, &vaddr, &foffset);
    setup_load2_va_length(s, &vaddr, &foffset);
    setup_dynamic_va_length(s, &vaddr, &foffset);

    s->s_shstrtab->vaddr = 0;
    s->s_shstrtab->file_offset = foffset;

    foffset += s->s_shstrtab->size;

    return foffset;
}

void post_setup_output_file_va_length(elf_state_t *s) {
    relocation_entry_t *t;
    symbol_t *tsym;
    dyntag_t *dt;

    /* patching symbols */
    list_for_each_entry(tsym, &s->symbols, entry) {
        tsym->value += s->s_text->vaddr;
    }

    dt = list_first_entry(&s->dtags[DT_HASH], dyntag_t, entry);
    dt->value = s->s_hash->vaddr;

    dt = list_first_entry(&s->dtags[DT_STRTAB], dyntag_t, entry);
    dt->value = s->s_dynstr->vaddr;

    dt = list_first_entry(&s->dtags[DT_SYMTAB], dyntag_t, entry);
    dt->value = s->s_dynsym->vaddr;

    dt = list_first_entry(&s->dtags[DT_REL], dyntag_t, entry);
    dt->value = s->s_relplt->vaddr;

    dt = list_first_entry(&s->dtags[DT_RELSZ], dyntag_t, entry);
    dt->value = s->s_relplt->size;

    dt = list_first_entry(&s->dtags[DT_RELENT], dyntag_t, entry);
    dt->value = 8;

    dt = list_first_entry(&s->dtags[DT_STRSZ], dyntag_t, entry);
    dt->value = s->s_dynstr->size;

    dt = list_first_entry(&s->dtags[DT_SYMENT], dyntag_t, entry);
    dt->value = 16;
}

void section2shdr(section_t *section, Elf32_Shdr *shdr) {
    shdr->sh_name = section->name_index;
    shdr->sh_type = section->type;
    shdr->sh_flags = section->flags;
    shdr->sh_addr = section->vaddr;
    shdr->sh_offset = section->file_offset;
    shdr->sh_size = section->size;

    switch (section->type) {
    case SHT_DYNAMIC:
        shdr->sh_link = 7; /* dynstr index */
        shdr->sh_info = 0;
        break;

    case SHT_HASH:
        shdr->sh_link = 6; /* dynsym index */
        shdr->sh_info = 0;
        break;

    case SHT_REL:
        shdr->sh_link = 6;  /* dynsym index */
        shdr->sh_info = 11; /* plt index */
        break;

    case SHT_DYNSYM: case SHT_SYMTAB:
        shdr->sh_link = 7; /* dynstr index */
        shdr->sh_info = 0; /* no locals for now */
        break;

    default:
        shdr->sh_link = SHN_UNDEF;
        shdr->sh_info = 0;
    };

    shdr->sh_addralign = section->align;
    shdr->sh_entsize = section->entsize;
}

void write_section_headers(elf_state_t *s, char *p) {
    Elf32_Shdr shdr;
    section_t *section;

    /* writing null-entry */
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_type = SHT_NULL;
    memcpy(p, &shdr, sizeof(shdr));
    p += sizeof(shdr);

    list_for_each_entry(section, &s->sections, entry) {
        section2shdr(section, &shdr);
        memcpy(p, &shdr, sizeof(shdr));
        p += sizeof(shdr);
    }
}

void pheader2phdr(pheader_t *header, Elf32_Phdr *phdr) {
    phdr->p_type = header->type;
    phdr->p_offset = header->file_offset;
    phdr->p_vaddr = header->vaddr;
    phdr->p_paddr = header->vaddr;
    phdr->p_filesz = header->size;
    phdr->p_memsz = header->size;
    phdr->p_align = header->align;
}

void write_program_headers(elf_state_t *s, char *p) {
    Elf32_Phdr phdr;
    pheader_t *pheader;

    list_for_each_entry(pheader, &s->pheaders, entry) {
        pheader2phdr(pheader, &phdr);
        memcpy(p, &phdr, sizeof(phdr));
        p += sizeof(phdr);
    }
}

void write_sections(elf_state_t *s, char *begin) {
    section_t *section;

    list_for_each_entry(section, &s->sections, entry) {
        memcpy(begin + section->file_offset, 
            section->data, section->size);
    }
}

relocation_entry_t *relocation_find(elf_state_t *s, const char *name) {
    struct hlist_node *t;
    relocation_entry_t *rel;
    unsigned int hval = bobj_hash(name, strlen(name)) % HASH_BUCKET_COUNT;
    hlist_for_each_entry(rel, t, &s->rheads[hval], hentry) {
        if (!strcmp(rel->name, name)) {
            return rel;
        }
    }
    return NULL;
}

/* offset inside code-section, which needs patching */
void relocation_add(elf_state_t *s, const char *name, int offset) {
    unsigned int hval;
    relocation_entry_t *rel;
    patching_entry_t *pentry;

    rel = relocation_find(s, name);
    if (!rel) {
        rel = malloc(sizeof(relocation_entry_t) + strlen(name));
        INIT_LIST_HEAD(&rel->entry);
        INIT_HLIST_NODE(&rel->hentry);

        rel->plt_offset = s->s_plt->size;
        section_add(s->s_plt, 1 + 4);

        rel->got_plt_offset = s->s_gotplt->size;
        section_add(s->s_gotplt, 8);

        INIT_LIST_HEAD(&rel->patching_list);

        hval = bobj_hash(name, strlen(name)) % HASH_BUCKET_COUNT;

        list_add(&rel->entry, &s->relocation_list);
        hlist_add_head(&rel->hentry, &s->rheads[hval]);
    }

    pentry = malloc(sizeof(patching_entry_t));
    INIT_LIST_HEAD(&pentry->entry);
    pentry->offset = offset;

    list_add(&pentry->entry, &rel->patching_list);
}

void setup_interp(elf_state_t *s) {
    char *p;
    const char *interp_name = "/lib/ld-linux.so.2";
    p = section_add(s->s_interp, strlen(interp_name) + 1);
    memcpy(p, interp_name, strlen(interp_name) + 1);
}

void setup_symbols(elf_state_t *s) {
    int i, cnt, ind, sz;
    char *p;
    unsigned int h;
    symbol_t *t;
    Elf32_Word *hash_table, *pp;
    Elf32_Sym sym;

    cnt = 0;
    list_for_each_entry(t, &s->symbols, entry) {
        cnt++;
    }

    /* setting up hash section */
    hash_table = malloc(sizeof(Elf32_Word) * (HASH_BUCKET_COUNT + cnt));
    for (i = 0; i < cnt; ++i) {
        hash_table[i + HASH_BUCKET_COUNT] = STN_UNDEF;
    }

    i = 0;
    list_for_each_entry(t, &s->symbols, entry) {
        h = elf_hash(t->name);

        ind = hash_table[h % HASH_BUCKET_COUNT];
        pp = &hash_table[ind];

        while (*pp != STN_UNDEF) {
            pp = &hash_table[*pp];
        }

        *pp = i;

        ++i;
    }

    sz = sizeof(Elf32_Word) * (HASH_BUCKET_COUNT + cnt);
    section_expand(s->s_hash, sz);
    memcpy(s->s_hash->data, hash_table, sz);

    /* adding up symbols to string-table */
    list_for_each_entry(t, &s->symbols, entry) {
        t->name_index = s->s_dynstr->size;
        p = section_add(s->s_dynstr, strlen(t->name) + 1);
        memcpy(p, t->name, strlen(t->name) + 1);
    }

    /* adding symbol-headers */
    list_for_each_entry(t, &s->symbols, entry) {
        memset(&sym, 0, sizeof(sym));
        sym.st_name = t->name_index;
        sym.st_value = t->value;
        sym.st_size = t->size;
        sym.st_info = ELF32_ST_INFO(STB_GLOBAL, STT_FUNC);
        sym.st_other = 0;
        sym.st_shndx = 2; /* text-section */
    }
}

void setup_section_strtable(elf_state_t *s) {
    char *p;
    section_t *t;
    list_for_each_entry(t, &s->sections, entry) {
        t->name_index = s->s_shstrtab->size;
        p = section_add(s->s_shstrtab, strlen(t->name) + 1);
        memcpy(p, t->name, strlen(t->name) + 1);
    }
}

void setup_dynamic(elf_state_t *s) {
    int i;
    Elf32_Dyn dyn, *pdyn;
    dyntag_t *t;

    for (i = DT_NUM - 1; i >= 0; --i) {
        list_for_each_entry(t, &s->dtags[i], entry) {
            pdyn = (Elf32_Dyn*)section_add(s->s_dynamic, sizeof(dyn));
            pdyn->d_tag = t->type;
            pdyn->d_un.d_ptr = t->value;
        }
    }
}

void setup_relocation(elf_state_t *s) {
    Elf32_Addr v1, v2, v;
    Elf32_Rel *rp;
    patching_entry_t *p;
    relocation_entry_t *t;

    list_for_each_entry(t, &s->relocation_list, entry) {

        /* patching text-section */
        list_for_each_entry(p, &t->patching_list, entry) {

            v1 = (Elf32_Addr)s->s_text->vaddr + p->offset;
            v2 = (Elf32_Addr)s->s_plt->vaddr + t->plt_offset;

            v = v1 - v2;
            pack4(s->s_text->data + p->offset + 1, &v);
        }

        v1 = (Elf32_Addr)s->s_plt->vaddr + t->plt_offset;
        v2 = (Elf32_Addr)s->s_gotplt->vaddr + t->got_plt_offset;

        v = v1 - v2;
        pack4(s->s_plt->data + t->plt_offset + 1, &v);
    }

    /* preparing relplt section */
    list_for_each_entry(t, &s->relocation_list, entry) {
        t->plt_offset += s->s_plt->vaddr;
        t->got_plt_offset += s->s_gotplt->vaddr;

        rp = (Elf32_Rel *)section_add(s->s_relplt, sizeof(Elf32_Rel));
        rp->r_offset = t->got_plt_offset;
        rp->r_info = ELF32_R_INFO(0, R_386_JMP_SLOT);
    }
}

void print_debug_info(elf_state_t *s) {
    section_t *t;
    list_for_each_entry(t, &s->sections, entry) {
        fprintf(stderr, "section=%s, offset=%d va=%x size=%d\n", 
                t->name, t->file_offset, t->vaddr, t->size);
    }
}

int elf_output_file(elf_state_t *s, const char *output_file) 
{
    Elf32_Ehdr *ehdr;
    Elf32_Addr vaddr, v; 
    int flength, fd;
    char *p;

    unlink(output_file);

    setup_interp(s);
    setup_symbols(s);
    setup_section_strtable(s);

    flength = setup_output_file_va_length(s);
    post_setup_output_file_va_length(s);

    setup_dynamic(s);
    setup_relocation(s);

    if ((fd = open(output_file, O_CREAT | O_RDWR,
            S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH)) == -1
        || !(p = malloc(flength)))
    {
        perror("fd open failed: ");
        return 1;
    }

    ehdr = (Elf32_Ehdr *)p;

    memset(ehdr, 0, sizeof(*ehdr));

    memcpy(ehdr->e_ident, ELFMAG, sizeof(ELFMAG));
    ehdr->e_ident[4] = ELFCLASS32;
    ehdr->e_ident[5] = ELFDATA2LSB;
    ehdr->e_ident[6] = EV_CURRENT;

    ehdr->e_type = ET_EXEC;
    ehdr->e_machine = EM_386;
    ehdr->e_version = EV_CURRENT;
    ehdr->e_entry = s->s_text->vaddr;
    ehdr->e_phoff = s->program_header_offset;
    ehdr->e_shoff = s->section_header_offset;
    ehdr->e_flags = 0;
    ehdr->e_ehsize = sizeof(Elf32_Ehdr);
    ehdr->e_phentsize = sizeof(Elf32_Phdr);
    ehdr->e_phnum = 5;
    ehdr->e_shentsize = sizeof(Elf32_Shdr);
    ehdr->e_shnum = SECTION_COUNT + 1;
    ehdr->e_shstrndx = ehdr->e_shnum - 1;

    write_section_headers(s, p + ehdr->e_shoff);
    write_program_headers(s, p + ehdr->e_phoff);
    write_sections(s, p);

    write(fd, p, flength);
    close(fd);

    return 0;
}

int main() {
    elf_state_t state;
    elf_setup_state(&state);
    elf_output_file(&state, "a.out");
    print_debug_info(&state);
    elf_free_state(&state);
    return 0;
}
