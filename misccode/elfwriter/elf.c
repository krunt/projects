#include <elf.h>

#include <myclib/list.h>


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

typedef struct rel_s {
    struct list_head entry;
    Elf32_Addr value;
    int type;
} rel_t;

typedef struct elf_state_s {

    Elf32_Addr base_address;

    int section_header_offset;
    int program_header_offset;

    struct list_head pheaders;
    struct list_head sections;
    struct list_head symbols;
    struct list_head rel;

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

} elf_state_t;

pheader_t *pheader_new(int type, int flags, int align) {
    section_t *res = malloc(sizeof(pheader_t));
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

section_t *section_new(const char *name, int type, int flags, int align) {
    section_t *res = malloc(sizeof(section_t) + strlen(name));
    INIT_LIST_HEAD(&res->entry);
    res->data = NULL;
    res->size = 0;
    res->type = type;
    res->flags = flags;
    res->align = align;
    res->vaddr = 0;
    res->fileoffset = 0;
    memcpy(res->name, name, strlen(name));
    res->name[stlren(name)] = 0;
    return res;
}

void section_expand(section_t *section, int new_sz) {
    section->data = realloc(section->data, new_sz);
    section->size = new_sz;
}

char *section_add(section_t *section, int bytes) {
    int original_size = section->size;
    section_expand(section, bytes + original_size);
    return section->data[original_size];
}

symbol_t *symbol_new(const char *name, Elf32_Addr value) {
    symbol_t *res = malloc(sizeof(symbol_t) + strlen(name));
    INIT_LIST_HEAD(&res->entry);
    res->value = value;
    res->size = 0;
    res->vaddr = 0;
    memcpy(res->name, name, strlen(name));
    res->name[stlren(name)] = 0;
    return res;
}

dyntag_t *dyntag_new(int type) {
    dyntag_t *res = malloc(sizeof(dyntag_t));
    INIT_LIST_HEAD(&res->entry);
    res->type = type;
    res->value = 0;
    return res;
}

rel_t *rel_new(int type) {
    rel_t *res = malloc(sizeof(rel_t));
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
    INIT_LIST_HEAD(&s->rel);

    s->s_interp = section_new(".interp", SHT_PROGBITS, SHF_ALLOC, 1);
    s->s_text = section_new(".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, 16);
    s->s_data = section_new(".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, 32);
    s->s_bss = section_new(".bss", SHT_NOBITS, SHF_ALLOC | SHF_WRITE, 32);
    s->s_dynamic = section_new(".dynamic", SHT_DYNAMIC, SHF_ALLOC | SHF_WRITE, 4);
    s->s_shstrtab = section_new(".shstrtab", SHT_STRTAB, 0, 1);
    s->s_dynsym = section_new(".dynsym", SHT_DYNSYM, SHF_ALLOC, 4);
    s->s_dynstr = section_new(".dynstr", SHT_STRTAB, SHF_ALLOC, 1);
    s->s_hash = section_new(".hash", SHT_HASH, SHF_ALLOC, 4);
    s->s_got = section_new(".got", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, 4);
    s->s_relplt = section_new(".rel.plt", SHT_REL, SHF_ALLOC, 4);
    s->s_plt = section_new(".plt", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, 4);
    s->s_gotplt = section_new(".got.plt", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, 0);

    /* after changing this order - change indices in section2shdr */
    list_add_tail(entry(s->s_interp), &s->sections);
    list_add_tail(entry(s->s_text), &s->sections);
    list_add_tail(entry(s->s_data), &s->sections);
    list_add_tail(entry(s->s_bss), &s->sections);
    list_add_tail(entry(s->s_dynamic), &s->sections);
    list_add_tail(entry(s->s_shstrtab), &s->sections);
    list_add_tail(entry(s->s_dynsym), &s->sections);
    list_add_tail(entry(s->s_dynstr), &s->sections);
    list_add_tail(entry(s->s_hash), &s->sections);
    list_add_tail(entry(s->s_got), &s->sections);
    list_add_tail(entry(s->s_relplt), &s->sections);
    list_add_tail(entry(s->s_plt), &s->sections);
    list_add_tail(entry(s->s_gotplt), &s->sections);

    s->p_phdr = pheader_new(PT_PHDR, PF_R | PF_E, 4);
    s->p_interp = pheader_new(PT_INTERP, PF_R, 1);
    s->p_load1 = pheader_new(PT_LOAD, PF_R | PF_X, 0x1000);
    s->p_load2 = pheader_new(PT_LOAD, PF_R | PF_W, 0x1000);
    s->p_dynamic = pheader_new(PT_DYNAMIC, PF_R | PF_W, 4);

    list_add_tail(entry(s->p_phdr), &s->pheaders);
    list_add_tail(entry(s->p_interp), &s->pheaders);
    list_add_tail(entry(s->p_load1), &s->pheaders);
    list_add_tail(entry(s->p_load2), &s->pheaders);
    list_add_tail(entry(s->p_dynamic), &s->pheaders);

    list_add_tail(entry(s->s_interp), &s->p_interp->sections);
    list_add_tail(entry(s->s_text), &s->p_load1->sections);
    list_add_tail(entry(s->s_got), &s->p_load1->sections);
    list_add_tail(entry(s->s_relplt), &s->p_load1->sections);
    list_add_tail(entry(s->s_plt), &s->p_load1->sections);
    list_add_tail(entry(s->s_gotplt), &s->p_load1->sections);
    list_add_tail(entry(s->s_dynsym), &s->p_load1->sections);
    list_add_tail(entry(s->s_dynstr), &s->p_load1->sections);
    list_add_tail(entry(s->s_hash), &s->p_load1->sections);
    list_add_tail(entry(s->s_data), &s->p_load2->sections);
    list_add_tail(entry(s->s_bss), &s->p_load2->sections);
    list_add_tail(entry(s->s_dynamic), &s->p_dynamic->sections);

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

void setup_plt_section(elf_state_t *s, Elf32_Addr *vaddr, int *len) {
    rel_t *t;

    list_for_each_entry(t, &s->rel, entry) {
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

void setup_hash_section(elf_state_t *s, Elf32_Addr *vaddr, int *len);

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

    setup_plt_section(s, vaddr, foffset);
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

    setup_hash_section(s, vaddr, &foffset);
    round_up(*vaddr, s->s_hash);
    round_up(*foffset, s->s_hash);

    s->p_load1.size = *foffset - orig_offset;

    round_up(*vaddr, s->p_load1);
    round_up(*foffset, s->p_load1);
}

void setup_load2_va_length(elf_state_t *s, Elf32_Addr *vaddr, int *foffset) {
    int orig_offset = *foffset;

    s->p_load2->vaddr = *vaddr;
    s->p_load2->file_offset = *foffset;

    *vaddr += aligned_length(s->s_data);
    *foffset += aligned_length(s->s_data);

    *vaddr += aligned_length(s->s_bss);
    //*foffset += aligned_length(s->s_bss);

    s->p_load2.size = *foffset - orig_offset;

    round_up(*vaddr, s->p_load2);
    round_up(*foffset, s->p_load2);
}

void setup_dynamic_va_length(elf_state_t *s, Elf32_Addr *vaddr, int *foffset) {
    int orig_offset = *foffset;

    s->p_dynamic->vaddr = *vaddr;
    s->p_dynamic->file_offset = *foffset;

    setup_dynamic_section(s, *vaddr, *foffset);

    s->p_dynamic.size = *foffset - orig_offset;

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
    foffset += sizeof(Elf32_Shdr) * (11 + 1);

    s->program_header_offset = foffset;
    foffset += sizeof(Elf32_Phdr) * 5;

    setup_phdr_va_length(&vaddr, &foffset);
    setup_interp_va_length(&vaddr, &foffset);
    setup_load1_va_length(&vaddr, &foffset);
    setup_load2_va_length(&vaddr, &foffset);
    setup_dynamic_va_length(&vaddr, &foffset);

    return foffset;
}

void post_setup_output_file_va_length(elf_state_t *s) {
    rel_t *t;
    symbol_t *tsym;
    dyntag_t *dt;

    /* patching plt-entries */
    list_for_each_entry(t, &s->rel, entry) {
        t->value += s->gotplt->vaddr;
    }

    /* and symbols */
    list_for_each_entry(tsym, &s->symbols, entry) {
        tsym->value += s->text->vaddr;
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
        shdr->sh_link = 8; /* dynstr index */
        shdr->sh_info = 0;
        break;

    case SHT_HASH:
        shdr->sh_link = 7; /* dynsym index */
        shdr->sh_info = 0;
        break;

    case SHT_REL:
        shdr->sh_link = 7;  /* dynsym index */
        shdr->sh_info = 12; /* plt index */
        break;

    case SHT_DYNSYM: case SHT_SYMTAB:
        shdr->sh_link = 8; /* dynstr index */
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
    memset(&shdr, 0, sizeof(shdr)):
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
    phdr->flags = header->align;
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

int elf_output_file(elf_state_t *s, const char *output_file) 
{
    Elf32_Ehdr *ehdr;
    Elf32_Addr vaddr, v; 
    int flength, fd;
    char *p;

    flength = setup_output_file_va_length(s);
    post_setup_output_file_va_length(s);

    if ((fd = open(output_file, O_WRONLY)) == -1
        || (p = mmap(NULL, flength, PROT_READ | PROT_WRITE,
                    0, fd, 0)) == MAP_FAILED)
    {
        perror("fd open failed: ");
        return 1;
    }

    ehdr = (Elf32_Ehdr *)p;

    memset(ehdr, 0, sizeof(*ehdr));
    memcpy(ehdr->e_ident, ELFMAG, sizeof(ELFMAG));
    ehdr->e_type = ET_EXEC;
    ehdr->e_machine = EM_386;
    ehdr->e_version = EV_CURRENT;
    ehdr->e_entry = s->s_text->vaddr;
    ehdr->e_phoff = s->program_header_offset;
    ehdr->e_shoff = s->section_header_offset;
    ehdr->e_flags = 0;
    ehdr->ehsize = sizeof(Elf32_Ehdr);
    ehdr->phentsize = sizeof(Elf32_Phdr);
    ehdr->phnum = 5;
    ehdr->shentsize = sizeof(Elf32_Shdr);
    ehdr->shnum = 11 + 1;
    ehdr->shstrndx = ehdr->shnum - 1;

    write_section_headers(s, p + ehdr->e_shoff);
    write_program_headers(s, p + ehdr->e_phoff);

    return 0;
}

