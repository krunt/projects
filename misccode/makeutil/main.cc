#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <sys/wait.h>

#include <string>
#include <vector>
#include <map>
#include <fstream>

typedef struct allocation_pool_s {
    char *base;
    char *top;
    int size;
} allocation_pool_t;

allocation_pool_t standard_pool;

static void
init_pool(allocation_pool_t *pool, int size = 1<<20) {
    pool->base = (char*)malloc(size);
    pool->top = pool->base;
    pool->size = size;
}

static char*
allocate_from_pool(allocation_pool_t *pool, size_t size) {
    char *old_top;
    size_t remaining = pool->size - (pool->top - pool->base);
    if (remaining < size) {
        pool->base = (char*)realloc(pool->base, pool->size + (size - remaining)); 
        pool->top = pool->base + (pool->size - remaining);
        pool->size += size - remaining;
    }
    old_top = pool->top;
    pool->top += size;
    return old_top;
}

static void
deallocate_pool(allocation_pool_t *pool) {
    free(pool->base);
}

enum marker_value {
    NOT_TO_EXECUTE,
    PENDING_TO_EXECUTE,
    TO_EXECUTE,
};

typedef struct target_execute_info_s {
    marker_value marker;
    int incount;
} target_execute_info_t;

struct node_s;
typedef struct target_s {
    char *name;
    char *cmdline;
    target_execute_info_t exec_info;

    struct node_s *parents;
    struct node_s *children;
} target_t;

typedef struct node_s {
    target_t *value;
    struct node_s *next;
} node_t;

typedef struct target_graph_s {
    std::map<std::string, target_t*> *target_map;
    node_t *targets;
} target_graph_t;

enum parse_state {
    TARGET_PART,
    AFTER_TARGET_PART,
    PARENT_TARGETS_PART,
    PRECOMMAND_PART,
    COMMAND_PART,
};

typedef struct parsing_context_s {
    parse_state state;
    char *begin;
    char *curr;
    char *end;

    int rownum;
    int colnum;

    target_graph_t *graph;
} parsing_context_t;

static parsing_context_t*
alloc_parsing_context(char *makefile_body) {
    parsing_context_t *cn = (parsing_context_t*)allocate_from_pool(
        &standard_pool, sizeof(parsing_context_t));
    cn->state = TARGET_PART;
    cn->begin = cn->curr = makefile_body;
    cn->end = cn->begin + strlen(cn->begin);
    cn->rownum = 0;
    cn->colnum = 0;
    cn->graph = NULL;
}

static void
free_parsing_context(parsing_context_t *cn) {
    /* empty */
}

static node_t*
alloc_node(target_t *target) {
    node_t *node = (node_t *)allocate_from_pool(&standard_pool, sizeof(node_t));
    node->value = target;
    node->next = NULL;
    return node;
}

static void
free_node(node_t *node) {
    /* empty */
}

static target_t*
alloc_target() {
    target_t *target = (target_t *)allocate_from_pool(&standard_pool, sizeof(target_t));
    target->name = NULL;
    target->cmdline = NULL;
    target->exec_info.marker = NOT_TO_EXECUTE;
    target->exec_info.incount = 0;
    target->parents = NULL;
    target->children = NULL;
    return target;
}

static void
free_target(target_t *target) {
    /* empty */
}

static node_t*
link_after(node_t *after_this, node_t *node) {
    node->next = after_this->next;
    after_this->next = node;
    return after_this;
}

static node_t*
list_insert(node_t **head, node_t *node) {
    if (!*head) {
        *head = node;
    } else {
        link_after(*head, node);
    }
    return *head;
}

static void
parse_error(parsing_context_t *cn, const char *message) {
    fprintf(stderr, "Parse error at %d.%d: %s\n", cn->rownum, cn->colnum, message);
    exit(1); 
}

static void
logic_error(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(2);
}

static void
system_error(const char *message) {
    perror(message);
    exit(3);
}

static char*
parse_file_bloat(const char *filename) {
    struct stat stat;
    FILE *fd = fopen(filename, "rb");
    if (!fd || fstat(fileno(fd), &stat)) {
        system_error("fopen failed:");
    }
    char *result = (char *)allocate_from_pool(&standard_pool, stat.st_size + 2); 
    if (!result) {
        system_error("realloc failed:");
    }
    if (fread(result, 1, stat.st_size, fd) != stat.st_size) {
        system_error("fread failed:");
    }
    /* adding for successful parse */
    result[stat.st_size] = '\n'; 
    result[stat.st_size + 1] = 0;
    return result;
}

static int
parse_isspace(char c) {
    return c == '\t' || c == ' ';
}

static int
parse_skip_whitespace(parsing_context_t *cn) {
    int skipped = 0;
    char **p = &cn->curr;
    while (*p != cn->end && parse_isspace(**p)) {
        (*p)++;
        cn->colnum++;
        skipped++;
    }
    return skipped;
}

static int target_symbols_table[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
};

static char*
parse_target_name(parsing_context_t *cn) {
    char **p = &cn->curr;
    char *str = *p;

    /* reading target name */
    while (*p != cn->end && target_symbols_table[(**p) & 0xFF]) {
        (*p)++; 
        cn->colnum++; 
    }

    if (*p == cn->end || (parse_isspace(**p) || **p == '\n' || **p == ':')) {
        if (**p == '\n') {
            cn->colnum = 0;
            cn->rownum++;
        } else {
            cn->colnum++;
        }

        if ((*p == cn->end || **p == '\n') && cn->state == PARENT_TARGETS_PART) {
            cn->state = PRECOMMAND_PART;
        } else if (**p == ':' && cn->state == TARGET_PART) {
            cn->state = PARENT_TARGETS_PART;
        } else {
            cn->state = AFTER_TARGET_PART;
        }

        **p = 0;
        (*p)++;
        return !*str ? NULL : str;
    }
    return NULL;
}

static void
parse_flush_target(parsing_context_t *cn, target_t *target, node_t *parents) {
    target->parents = parents;
    list_insert(&cn->graph->targets, alloc_node(target));
    cn->state = TARGET_PART;
    (*cn->graph->target_map)[std::string(target->name)] = target;
}

static void
parse_postprocess(parsing_context_t *cn);

static target_graph_t*
parse_makefile(char *makefile_body) {
    int skipped_whitespaces;
    char *target_name = NULL;
    char *parent_target_name = NULL;
    char *target_command = NULL;
    node_t *prev_pnode = NULL;
    target_t *curtarget;
    parsing_context_t *cn = alloc_parsing_context(makefile_body);
    char **p = &cn->curr;

    cn->graph = (target_graph_t *)allocate_from_pool(
            &standard_pool, sizeof(target_graph_t));
    cn->graph->target_map = new std::map<std::string, target_t*>();
    cn->graph->targets = NULL;

    while (*p < cn->end) {
        skipped_whitespaces = parse_skip_whitespace(cn);

        /* skipping empty lines */
        if (cn->state == TARGET_PART && **p == '\n') {
            (*p)++;
            prev_pnode = NULL;
            continue;
        }

        switch (cn->state) {
        case TARGET_PART: { 
            curtarget = alloc_target();
            if (!(target_name = parse_target_name(cn))) {
                parse_error(cn, "target name is empty or invalid");
            }
            curtarget->name = target_name;
            break;
        }
    
        case AFTER_TARGET_PART: {
            if (*p == cn->end || **p != ':') {
                parse_error(cn, "expected `:` symbol");
            }
            cn->state = PARENT_TARGETS_PART;
            (*p)++; /* skipping `:` */
            break;
        }
    
        case PARENT_TARGETS_PART: {
            while (cn->state != PRECOMMAND_PART) {
                parent_target_name = parse_target_name(cn);
    
                if (!parent_target_name && cn->state != PRECOMMAND_PART) {
                    parse_error(cn, "can't parse parents_target");
                }

                if (parent_target_name) {
                    /* here is a hack */
                    node_t *pnode = alloc_node((target_t *)parent_target_name);
                    list_insert(&prev_pnode, pnode);
                }

                skipped_whitespaces = parse_skip_whitespace(cn);
            }
    
            assert(cn->state == PRECOMMAND_PART);
        }
    
        /* fall through */
        case PRECOMMAND_PART: {
            if (skipped_whitespaces) {
                cn->state = COMMAND_PART; 
            } else {
                /* there is no command */
                parse_flush_target(cn, curtarget, prev_pnode);
                prev_pnode = NULL;
            }
            break;
        }
    
        case COMMAND_PART: {
            char **p = &cn->curr;
            target_command = *p;
            while (*p != cn->end && **p != '\n') {
                cn->colnum++;
                (*p)++;
            }
            **p = 0;
            (*p)++;

            curtarget->cmdline = target_command;
            parse_flush_target(cn, curtarget, prev_pnode);
            prev_pnode = NULL;
            break;
        }};
    }
    parse_postprocess(cn);
    free_parsing_context(cn);
    return cn->graph;
}

static void
parse_postprocess(parsing_context_t *cn) {
    target_graph_t *graph = cn->graph;
    node_t *target = graph->targets;
    std::map<std::string, target_t*>::iterator it;
    while (target) {
        target_t *target_value = target->value;
        node_t *parent = target_value->parents;
        target_value->parents = NULL;
        while (parent) {
            if ((it = cn->graph->target_map->find(std::string((char*)parent->value)))
                    == cn->graph->target_map->end()) 
            { logic_error("unknown target `%s`", parent->value); }

            target_t *to_insert = (*it).second;
            list_insert(&target_value->parents, alloc_node(to_insert));
            list_insert(&to_insert->children, alloc_node(target_value));

            target_value->exec_info.marker = NOT_TO_EXECUTE;
            target_value->exec_info.incount = 0;

            parent = parent->next;
        }
        target = target->next;
    }
}

/* TODO: lookup hash-table is needed here */
static void 
mark_build_nodes(target_t *target) {
    node_t *child = target->children;
    target->exec_info.marker = PENDING_TO_EXECUTE;
    while (child) {
        mark_build_nodes(child->value);
        child = child->next;
    }
}

static void 
mark_updated_nodes(target_t *target) {
    if (target->exec_info.marker == PENDING_TO_EXECUTE) {
        target->exec_info.marker = TO_EXECUTE;
    }
    node_t *parent = target->parents;
    while (parent) {
        mark_updated_nodes(parent->value);
        parent = parent->next;
    }
}

static node_t*
find_ready_node_with_zero_incount(node_t *node) {
    while (node) {
        if (node->value->exec_info.marker == TO_EXECUTE
            && (!node->value->exec_info.incount))
        {
            return alloc_node(node->value);
        }
        node = node->next;
    }
    return NULL;
}

static node_t*
find_ready_node_with_non_zero_incount(node_t *node) {
    while (node) {
        if (node->value->exec_info.marker == TO_EXECUTE
            && (node->value->exec_info.incount > 0))
        {
            return alloc_node(node->value);
        }
        node = node->next;
    }
    return NULL;
}

static void
reduce_incount_value(node_t *node) {
    while (node) {
        if (node->value->exec_info.marker == TO_EXECUTE) {
            --node->value->exec_info.incount;
        }
        node = node->next;
    }
}

static void
print_dependency_node(node_t *node, int indent) {
    if (node->value->exec_info.marker == TO_EXECUTE) {
        for (int i = 0; i < indent; ++i) { printf(" "); }
        printf("%s: (in=%d)\n", node->value->name, node->value->exec_info.incount);
    } 
    node = node->value->children;
    while (node) {
        print_dependency_node(node, indent + 2);
        node = node->next;
    }
}

static void
print_dependency_graph(target_graph_t *graph) {
    printf("---Schedule graph---\n");
    node_t *node = graph->targets;
    while (node) {
        print_dependency_node(node, 0);
        printf("\n");
        node = node->next;
    }
    printf("--------------------\n");
}

static node_t*
generate_schedule_queue(target_graph_t *graph,
    const std::vector<std::string> &build_target_names,
    const std::vector<std::string> &updated_target_names)
{
    if (build_target_names.size() == 1 && build_target_names[0] == "all") {
        node_t *node = graph->targets; 
        while (node) {
            mark_build_nodes(node->value);
            node = node->next;
        }
    } else {
        /* marking first */
        for (int i = 0; i < build_target_names.size(); ++i) {
            const std::string &build_target_name = build_target_names[i];
            if (graph->target_map->find(build_target_name) == graph->target_map->end()) {
                logic_error("Target `%s` is not found", build_target_name.c_str());
            }
            mark_build_nodes((*graph->target_map)[build_target_name]);
        }
    }

    if (updated_target_names.size() == 1 && updated_target_names[0] == "all") {
        node_t *node = graph->targets; 
        while (node) {
            mark_updated_nodes(node->value);
            node = node->next;
        }
    } else {
        for (int i = 0; i < updated_target_names.size(); ++i) {
            const std::string &updated_target_name = updated_target_names[i];
            if (graph->target_map->find(updated_target_name) == graph->target_map->end()) {
                logic_error("Target `%s` is not found", updated_target_name.c_str());
            }
            mark_updated_nodes((*graph->target_map)[updated_target_name]);
        }
    }

    /* initializing incounts */
    node_t *node = graph->targets; 
    while (node) {
        node_t *parent = node->value->parents;
        while (parent) {
            if (node->value->exec_info.marker == TO_EXECUTE 
                && parent->value->exec_info.marker == TO_EXECUTE) 
            { node->value->exec_info.incount++; }
            parent = parent->next;
        }
        node = node->next;
    }

    node_t *result_head = NULL, *current_head = NULL;
    while (1) {
        node_t *min_node = find_ready_node_with_zero_incount(graph->targets);
        if (!min_node) {
            break;
        }

        if (current_head) {
            link_after(current_head, min_node);
            current_head = current_head->next;
        } else {
            result_head = current_head = min_node;
        }

        /* setting non-zero incount, 
         * so next iterations won't choose it again */
        min_node->value->exec_info.incount = -1; 
        reduce_incount_value(min_node->value->children);
    }

    /* checking cycles */
    node_t *node_with_cycle;
    if ((node_with_cycle = find_ready_node_with_non_zero_incount(graph->targets))) {
        logic_error("Cycles in dependencies: one of them in `%s`", 
                node_with_cycle->value->name);
    }
    return result_head;
}

static void
print_schedule_queue(node_t *node) {
    printf("---Schedule queue---\n");
    while (node) {
        printf("%s: %c%s%c\n", node->value->name,
            node->value->cmdline ? '`' : '<',
            node->value->cmdline ? node->value->cmdline : "none",
            node->value->cmdline ? '`' : '>');
        node = node->next;
    }
    printf("--------------------\n");
}

static pid_t 
execute_process(char *cmd_name) {
    pid_t result;
    if ((result = fork()) == 0) {
        std::vector<char *> args;
        char *p = cmd_name;
        while (*p) {
            while (*p && isspace(*p)) { p++; }
            if (*p) args.push_back(p);
            while (*p && !isspace(*p)) { p++; }
            if (!*p) break;
            *p++ = 0;
        }
        args.push_back(NULL);
        execv(args[0], &args[0]);
        exit(3);
    } 
    if (result == -1) { system_error("fork failed:"); }
    return result;
}

static void
wait_for_any_child() {
    int status;
    if (waitpid(-1, &status, 0) == -1) {
        system_error("waitpid failed:");
    }
    if (WEXITSTATUS(status)) {
        logic_error("child exited with `%d` value", WEXITSTATUS(status));
    }
}

static void
execute_schedule_queue(node_t *node, int parallel) {
    int currently_free = parallel;
    while (node) {
        /* skip node with no command */
        if (!node->value->cmdline) {
            node = node->next;
            continue;
        }

        if (currently_free) {
            execute_process(node->value->cmdline);
            currently_free--;
        }

        if (!currently_free) {
            wait_for_any_child();
            currently_free++;
        }
        node = node->next;
    }

    /* final waiting stage */
    while (currently_free < parallel) {
        wait_for_any_child();
        currently_free++;
    }
}

static void
init_updated_target_names(std::vector<std::string> &updated_target_names) {
    const char *updated_filename = ".updated";
    if (access(updated_filename, R_OK | W_OK)) {
        return; /* no file or no rights to it (not-error) */
    }

    std::fstream fs(updated_filename, std::ios::in);
    updated_target_names.clear();
    std::string fname; time_t ktime;
    while (fs >> fname >> ktime) {
        struct stat st;
        if (stat(fname.c_str(), &st)) {
            system_error("stat failed:");
        }
        if (ktime != st.st_atime) {
            updated_target_names.push_back(fname);
        }
    }
}

int main() {
    init_pool(&standard_pool);

    int dryrun = 1, parallel = 1;
    const char *makefile_filename = "Makefile";
    std::vector<std::string> build_target_names, updated_target_names;
    build_target_names.push_back("all");
    updated_target_names.push_back("all");
    init_updated_target_names(updated_target_names);
    
    char *file_contents = parse_file_bloat(makefile_filename);
    target_graph_t *graph = parse_makefile(file_contents);
    node_t *queue = generate_schedule_queue(graph, 
            build_target_names, updated_target_names);
    print_dependency_graph(graph);
    if (dryrun) { print_schedule_queue(queue); }
    else { execute_schedule_queue(queue, parallel); }

    delete graph->target_map;
    deallocate_pool(&standard_pool);
    return 0;
}
