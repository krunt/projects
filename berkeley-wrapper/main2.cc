
#include <boost/preprocessor/arithmetic.hpp>
#include <boost/preprocessor/comparison/not_equal.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/comparison.hpp>
#include <boost/preprocessor/punctuation/comma.hpp>
#include <boost/preprocessor/stringize.hpp>

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/repetition.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/operators.hpp>

#include <boost/mpl/vector.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/at.hpp>
#include <boost/mpl/int.hpp>

#include <iostream>

#include <string>
#include <iterator>
#include <string.h>
#include <assert.h>
#include <vector>

#include <db_cxx.h>

class Field {
public:
    virtual ~Field() {}
    virtual void dispose() = 0;
    virtual int length() const = 0;
    virtual int min_packed_length() const = 0;
    virtual int packed_length() const = 0;
    virtual char *pack(char *data) const = 0;
    virtual char *unpack(char *data) = 0;
    virtual int compare(const Field &rhs) const = 0;
    virtual std::string to_printable_string() const = 0;
};

class Int32: public Field {
public:
    Int32() : num_(0)
    {}

    Int32(int32_t num)
        : num_(num)
    {}

    void dispose() {
    }

    char *pack(char *data) const {
        *reinterpret_cast<int32_t*>(data) = num_;
        return data + 4;
    }

    char *unpack(char *data) {
        num_ = *reinterpret_cast<int32_t*>(data);
        return data + 4;
    }

    int length() const {
        return 4;
    }

    int min_packed_length() const {
        return 4;
    }

    int packed_length() const {
        return 4;
    }

    int compare(const Field &other) const {
        const Int32 &casted_other = static_cast<const Int32 &>(other);
        return num_ - casted_other.num_;
    }

    std::string to_printable_string() const {
        return boost::lexical_cast<std::string>(num_);
    }

private:
    int32_t num_;
};

static char hexmap[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

class String: public Field {
public:
    String() : data_(NULL), len_(0), capacity_(0)
    {}

    String(char *data, int len)
        : data_(data), len_(len), capacity_(len)
    {}

    void dispose() {
        if (data_) {
            free(data_);
            data_ = NULL;
            len_ = 0;
            capacity_ = 0;
        }
    }

    char *pack(char *data) const {
        *reinterpret_cast<int32_t*>(data) = len_;
        if (len_) {
            memcpy(data + 4, data_, len_);
        }
        return data + 4 + len_;
    }

    char *unpack(char *data) {
        int packed_length = *reinterpret_cast<int32_t*>(data);
        if (packed_length > capacity_) {
            enlarge(packed_length);
        }
        len_ = packed_length;
        if (len_) { memcpy(data_, data + 4, len_); }
        return data + 4 + len_;
    }

    char *data() const {
        return data_;
    }

    int length() const {
        return len_;
    }

    int min_packed_length() const {
        return 4;
    }

    int packed_length() const {
        return 4 + len_;
    }

    void set_length(int newlen) {
        len_ = newlen;
    }

    int compare(const Field &other) const {
        const String &casted_other = static_cast<const String &>(other);
        int result = memcmp(data_, casted_other.data_, 
                std::min(length(), casted_other.length()));
        int cmpresult = result ? result : length() - casted_other.length();
        /*
        std::cerr << to_printable_string() << " cmp " << other.to_printable_string() 
            << " = " << boost::lexical_cast<std::string>(cmpresult) << std::endl; 
            */
        return cmpresult;
    }

    void enlarge(int needed_capacity) {
        if (needed_capacity > capacity_) {
            data_ = static_cast<char*>(realloc(data_, needed_capacity));
            capacity_ = needed_capacity;
        }
    }

    std::string to_printable_string() const {
        std::string result;
        for (int i = 0; i < len_; ++i) {
            if (isprint(data_[i])) {
                result.append(1, data_[i]);
            } else {
                result.append(1, '\\');
                result.append(1, 'x');
                result.append(1, hexmap[(data_[i] >> 4) & 0x0F]);
                result.append(1, hexmap[data_[i] & 0x0F]);
            }
        }
        return result;
    }

private:
    char *data_;
    int len_;
    int capacity_;
};

String mkstring(const char *name) {
    return String(const_cast<char*>(name), strlen(name));
}

String *mknewbufstring(int len) {
    char *p = static_cast<char*>(malloc(len));
    return new String(p, len);
}

String *mknewstring(const char *name) {
    int len = strlen(name);
    char *p = static_cast<char*>(malloc(len));
    memcpy(p, name, len);
    return new String(p, len);
}

String *mknewstring(const char *name, int len) {
    char *p = static_cast<char*>(malloc(len));
    memcpy(p, name, len);
    return new String(p, len);
}

class BdbCursor {
public:
    BdbCursor(Dbc *cursorp) 
        : cursorp_(cursorp)
    {}

    ~BdbCursor() {
        cursorp_->close();
    }

    void current(String &key, String &value) {
        key = String(static_cast<char*>(key_.get_data()), key_.get_size());
        value = String(static_cast<char*>(value_.get_data()), value_.get_size());
    }

    bool first() {
        return cursorp_->get(&key_, &value_, DB_FIRST) != DB_NOTFOUND;
    }

    bool last() {
        return cursorp_->get(&key_, &value_, DB_LAST) != DB_NOTFOUND;
    }

    bool step() {
        return cursorp_->get(&key_, &value_, DB_NEXT) != DB_NOTFOUND;
    }

    bool step_backward() {
        return cursorp_->get(&key_, &value_, DB_PREV) != DB_NOTFOUND;
    }

    bool first_key_exact(const String &key) {
        key_ = Dbt(key.data(), key.length());
        return cursorp_->get(&key_, &value_, DB_SET) != DB_NOTFOUND;
    }

    bool first_key_range(const String &key) {
        key_ = Dbt(key.data(), key.length());
        return cursorp_->get(&key_, &value_, DB_SET_RANGE) != DB_NOTFOUND;
    }

private:
    Dbt key_;   /* cached key */
    Dbt value_; /* cached value */
    Dbc *cursorp_;
};

template <typename IndexFields>
class BdbImpl {
public:
    enum { INITIAL_KEY_CAPACITY = 1024 };

public:
    BdbImpl(const std::string &filename) 
        : db_(NULL, 0)
    {
        db_.set_error_stream(&std::cerr);
        db_.set_app_private(static_cast<void*>(this));
        db_.set_bt_compare(&btree_compare_function);
        db_.open(NULL, filename.c_str(), NULL, DB_BTREE, 
                DB_CREATE, 0);
        cached_key_ = String(static_cast<char*>(malloc(INITIAL_KEY_CAPACITY)), 
                INITIAL_KEY_CAPACITY);
    }

    ~BdbImpl() {
        db_.close(0);
        free(cached_key_.data());
    }

    void insert_impl(const String &key, const String &value) {
        Dbt dbkey(key.data(), key.length()); 
        Dbt dbvalue(value.data(), value.length()); 
        db_.put(NULL, &dbkey, &dbvalue, 0);
    }

    void enlarge_key_if_needed(int needed_capacity) {
        cached_key_.enlarge(needed_capacity);
    }

    String &cached_key() { return cached_key_; }

    BdbCursor *get_bdb_cursor() {
        Dbc *result;
        db_.cursor(NULL, &result, 0);
        return new BdbCursor(result);
    }

    static int btree_compare_function(Db *db, const Dbt *dbt1, const Dbt *dbt2);

private:
    Db db_;
    String cached_key_;
};

namespace mpl = boost::mpl;

class FullTableScanIterator
    : public std::iterator<std::bidirectional_iterator_tag,
        std::pair<const String, const String> >,
        public boost::equality_comparable1<FullTableScanIterator>,
        public boost::incrementable<FullTableScanIterator>
{
public:
    FullTableScanIterator(BdbCursor *cursor, bool at_end) 
         : cursor_(cursor), at_end_(at_end)
    { 
        if (!cursor_->first()) { 
            at_end_ = true; 
        }
    }

    ~FullTableScanIterator() {
        delete cursor_;
    }

    FullTableScanIterator &operator++() {
        assert(!at_end_);

        if (!cursor_->step()) { 
            at_end_ = true; 
        }
        return *this;
    }

    value_type operator*() {
        String key, value;
        cursor_->current(key, value);
        return value_type(key, value);
    }

    bool operator==(const FullTableScanIterator &rhs) const {
        return at_end_ == rhs.at_end_;
    }

private:
    BdbCursor *cursor_;
    bool at_end_;
};


class MinMaxFieldPredicate {
public:
    MinMaxFieldPredicate()
        : min_(NULL), max_(NULL)
    {}

    bool eligible_for_prefix() const {
        return min_ && max_;
    }

    bool impossible_condition() const {
        return eligible_for_prefix() && min_->compare(*max_) > 0;
    }

    Field *get_zero() const {
        return zero_;
    }

    Field *get_min() const {
        return min_;
    }

    Field *get_max() const {
        return max_;
    }

    void set_zero(Field *field) {
        zero_ = field;
    }

    void set_min(Field *field) {
        min_ = field;
    }

    void set_max(Field *field) {
        max_ = field;
    }

    char *pack_zero(char *data) const {
        assert(zero_);
        return zero_->pack(data);
    }

    char *pack_min(char *data) const {
        return min_->pack(data);
    }

    char *pack_max(char *data) const {
        return max_->pack(data);
    }

    std::string to_string() const {
        std::string result = "[";
        if (min_) { result += min_->to_printable_string(); }
        result += ", ";
        if (max_) { result += max_->to_printable_string(); }
        result += "], ";
        return result;
    }

private:
    Field *zero_;
    Field *min_;
    Field *max_;
};


template <typename IndexFields>
class Predicate {
public:
    typedef std::vector<Field*> fieldlist_type;
    typedef std::vector<MinMaxFieldPredicate> field_predicatelist_type;

public:
    Predicate() {
        predicate_list_.resize(mpl::size<IndexFields>::value);
    }

    Predicate(const field_predicatelist_type &predicates) 
        : predicate_list_(predicates)
    {}

    bool impossible() const {
        int i = 0;
        while (i < predicate_list_.size()) {
            if (predicate_list_[i].impossible_condition()) {
                return true;
            }
            ++i;
        }
        return false;
    }

    int prefix_field_length() const {
        int i = 0;
        while (i < predicate_list_.size() && predicate_list_[i].eligible_for_prefix()) {
            ++i;
        }
        return i;
    }

    int prefixed_min_key_length() const {
        int result = 0;
        for (int i = 0, end = prefix_field_length(); i < end; ++i) {
            result += predicate_list_[i].get_min()->packed_length();
        }
        for (int i = prefix_field_length(); i < predicate_list_.size(); ++i) {
            result += predicate_list_[i].get_zero()->min_packed_length();
        }
        return result;
    }

    int prefixed_max_key_length() const {
        int result = 0;
        for (int i = 0, end = prefix_field_length(); i < end; ++i) {
            result += predicate_list_[i].get_max()->packed_length();
        }
        for (int i = prefix_field_length(); i < predicate_list_.size(); ++i) {
            result += predicate_list_[i].get_zero()->min_packed_length();
        }
        return result;
    }

    String make_min_key() {
        int len = prefixed_min_key_length();
        assert(len != 0);

        String *key = mknewbufstring(len);

        char *p = key->data();
        for (int i = 0, end = prefix_field_length(); i < end; ++i) {
            p = predicate_list_[i].pack_min(p);
        }
        for (int i = prefix_field_length(); i < predicate_list_.size(); ++i) {
            p = predicate_list_[i].pack_zero(p);
        }
        return *key;
    }

    String make_max_key() {
        int len = prefixed_max_key_length();
        assert(len != 0);

        String *key = mknewbufstring(len);

        char *p = key->data();
        for (int i = 0, end = prefix_field_length(); i < end; ++i) {
            p = predicate_list_[i].pack_max(p);
        }
        for (int i = prefix_field_length(); i < predicate_list_.size(); ++i) {
            p = predicate_list_[i].pack_zero(p);
        }
        return *key;
    }

    bool is_in_range(const fieldlist_type &fields) const {
        assert(predicate_list_.size() == fields.size());
        for (int i = 0; i < predicate_list_.size(); ++i) {
            const MinMaxFieldPredicate &minmax = predicate_list_[i];
            if (minmax.get_min() && fields[i]->compare(*minmax.get_min()) < 0) {
                return false;  
            }
            if (minmax.get_max() && fields[i]->compare(*minmax.get_max()) > 0) {
                return false;  
            }
        }
        return true;
    }

    std::string to_string() const {
        std::string result;
        for (int i = 0; i < predicate_list_.size(); ++i) {
            result += boost::lexical_cast<std::string>(i);
            result += predicate_list_[i].to_string();
        }
        return result;
    }

private:
    field_predicatelist_type predicate_list_;
};

template <typename IndexFields>
void dispose_fieldlist_shim(typename Predicate<IndexFields>::fieldlist_type &vec) {
    for (int i = 0; i < vec.size(); ++i) {
        vec[i]->dispose();
        delete vec[i];
        vec[i] = NULL;
    }
}

template <int index, typename IndexFields>
struct ZeroInitializer {
    typedef std::vector<MinMaxFieldPredicate> field_predicatelist_type;
    typedef typename mpl::at<IndexFields, mpl::int_<index> >::type ArgType;

    ZeroInitializer(field_predicatelist_type &predicate_list)
        : predicate_list_(predicate_list)
    {}

    void initialize() {
        predicate_list_[index].set_zero(new ArgType());
        ZeroInitializer<index - 1, IndexFields>(predicate_list_).initialize();
    }

    field_predicatelist_type &predicate_list_;
};

template <typename IndexFields>
struct ZeroInitializer<-1, IndexFields> {
    typedef std::vector<MinMaxFieldPredicate> field_predicatelist_type;
    ZeroInitializer(field_predicatelist_type &stub) {
    }
    void initialize() {}
};

template <typename IndexFields>
class PredicateEntity {
public:
    typedef std::vector<MinMaxFieldPredicate> field_predicatelist_type;

public:
    PredicateEntity() {
        predicate_list_.resize(mpl::size<IndexFields>::value);
        initialize_zeroes();
    }

    field_predicatelist_type &field_predicates() {
        return predicate_list_;
    }

    void initialize_zeroes() {
        ZeroInitializer<mpl::size<IndexFields>::value - 1, 
            IndexFields>(predicate_list_).initialize();
    }

private:
    field_predicatelist_type predicate_list_;
};




template <int index, typename IndexFields, typename T>
class ArgumentIMpl;


template <int index, typename IndexFields>
class Argument<index, IndexFields, String>
    : public PredicateEntity<IndexFields> 
{
public:
    typedef Argument<index, IndexFields, String> this_type;
    typedef typename PredicateEntity<IndexFields>::field_predicatelist_type 
        field_predicatelist_type;

public:

    /* merge operation */
    template <int index2, typename T>
    this_type &operator&&(Argument<index2, IndexFields, T> &argument) {
        field_predicatelist_type &mine = this->field_predicates();
        field_predicatelist_type &other = argument.field_predicates();
        for (int i = 0; i < other.size(); ++i) {
            if (other[i].get_max()) {
                replace_max_if_needed(i, *static_cast<const String *>(other[i].get_max()));
            }
            if (other[i].get_min()) {
                replace_min_if_needed(i, *static_cast<const String *>(other[i].get_min()));
            }
        }
        return *this;
    }

    this_type &operator<(std::string val) {
        replace_max_if_needed(index, String(const_cast<char*>(val.data()), val.size()));
        return *this;
    }

    this_type &operator>(std::string val) {
        replace_min_if_needed(index, String(const_cast<char*>(val.data()), val.size()));
        return *this;
    }

private:
    void replace_max_if_needed(int ind, const String &val) {
        MinMaxFieldPredicate &predicate = this->field_predicates()[ind];
        if (predicate.get_max() 
                && predicate.get_max()->compare(val) > 0)
        {
            predicate.get_max()->dispose();
            predicate.set_max(mknewstring(val.data(), val.length()));
        } else if (!predicate.get_max()) {
            predicate.set_max(mknewstring(val.data(), val.length()));
        }
    }

    void replace_min_if_needed(int ind, const String &val) {
        MinMaxFieldPredicate &predicate = this->field_predicates()[ind];
        if (predicate.get_min() 
                && predicate.get_min()->compare(val) < 0) 
        {
            predicate.get_min()->dispose();
            predicate.set_min(mknewstring(val.data(), val.length()));
        } else if (!predicate.get_min()) {
            predicate.set_min(mknewstring(val.data(), val.length()));
        }
    }
};

template <int index, typename IndexFields>
class Argument<index, IndexFields, Int32>
    : public PredicateEntity<IndexFields> 
{
public:
    typedef Argument<index, IndexFields, Int32> this_type;
    typedef typename PredicateEntity<IndexFields>::field_predicatelist_type 
        field_predicatelist_type;

public:
    /* merge operation */
    template <int index2, typename T>
    this_type &operator&&(Argument<index2, IndexFields, T> &argument) {
        field_predicatelist_type &mine = this->field_predicates();
        field_predicatelist_type &other = argument.field_predicates();
        for (int i = 0; i < other.size(); ++i) {
            if (other[i].get_max()) {
                replace_max_if_needed(i, *static_cast<const Int32*>(*other.get_max()));
            }
            if (other[i].get_min()) {
                replace_min_if_needed(i, *static_cast<const Int32*>(other[i].get_min()));
            }
        }
        return *this;
    }

    this_type &operator<(int val) {
        replace_max_if_needed(index, Int32(val));
        return *this;
    }

    this_type &operator>(int val) {
        replace_min_if_needed(index, Int32(val));
        return *this;
    }

private:
    void replace_min_if_needed(int ind, const Int32 &val) {
        MinMaxFieldPredicate &predicate = this->field_predicates()[ind];
        if (predicate.get_max() 
                && predicate.get_max()->compare(val) > 0)
        {
            predicate.get_max()->dispose();
            predicate.set_max(new Int32(val));
        } else if (!predicate.get_max()) {
            predicate.set_max(new Int32(val));
        }
    }

    void replace_max_if_needed(int ind, const Int32 &val) {
        MinMaxFieldPredicate &predicate = this->field_predicates()[ind];
        if (predicate.get_min() 
                && predicate.get_min()->compare(val) < 0) 
        {
            predicate.get_min()->dispose();
            predicate.set_min(new Int32(val));
        } else if (!predicate.get_min()) {
            predicate.set_min(new Int32(val));
        }
    }
};

template <typename IndexFields>
class PredicateBuilder {
public:
    typedef typename PredicateEntity<IndexFields>::field_predicatelist_type 
        field_predicatelist_type;

    template <int index>
    class Arg {
    public:
        typedef Argument<index, IndexFields,
            typename mpl::at<IndexFields, mpl::int_<index> >::type> type;
    };

public:

    template <int index>
    const field_predicatelist_type evaluate(typename Arg<index>::type arg) {
        return arg.field_predicates(); 
    }
};


template <typename IndexFields>
class Table;

template <typename IndexFields>
static void unpack_key_shim(Table<IndexFields> &table,
    typename Predicate<IndexFields>::fieldlist_type &fieldlist,
    const String &key);

template <typename IndexFields>
class FilterIterator
    : public std::iterator<std::bidirectional_iterator_tag,
        std::pair<const String, const String> >,
        public boost::equality_comparable1<FilterIterator<IndexFields> >,
        public boost::incrementable<FilterIterator<IndexFields> > 
{
public:
    typedef typename Predicate<IndexFields>::fieldlist_type fieldlist_type;

public:
    FilterIterator(Table<IndexFields> &table,
            BdbCursor *cursor, 
            Predicate<IndexFields> *predicate, 
            bool at_end)
         : table_(table), cursor_(cursor), 
         predicate_(predicate), at_end_(at_end), is_positioned_(false)
    { reset(); }

    ~FilterIterator() {
        if (cursor_) { delete cursor_; }
        if (predicate_) { delete predicate_; }
        if (is_positioned_) {
            free(minkey_.data());
            free(maxkey_.data());
        }
        dispose_fieldlist_shim<IndexFields>(fieldlist_);
    }

    void reset() {
        if (predicate_ && predicate_->impossible()) {
            at_end_ = true;
            return;
        }

        /* is there a full scan */
        if (!predicate_ || !predicate_->prefix_field_length()) {
            if (!cursor_->step()) {
                at_end_ = true;
            }
            return;
        }

        /* prefixed scan init */
        is_positioned_ = true;
        minkey_ = predicate_->make_min_key();
        maxkey_ = predicate_->make_max_key();

        if (!cursor_->first_key_range(minkey_)) {
            at_end_ = true;
        }
    }

    FilterIterator &operator++() {
        assert(!at_end_);

        if (!predicate_) {
            if (!cursor_->step()) {
                at_end_ = true;
            }
            return  *this;
        }

        do {
            if (!cursor_->step()) {
                at_end_ = true;
                return *this;
            }

            String key, value;
            cursor_->current(key, value);

            if (is_positioned_ && table_.compare_keys(key, maxkey_) > 0) {
                at_end_ = true;
                return *this;
            }

            unpack_key_shim(table_, fieldlist_, key);
        } while (!predicate_->is_in_range(fieldlist_));
        return *this;
    }

    value_type operator*() {
        String key, value;
        cursor_->current(key, value);
        return value_type(key, value);
    }

    bool operator==(const FilterIterator &rhs) const {
        return at_end_ == rhs.at_end_;
    }

private:
    Table<IndexFields> &table_;
    BdbCursor *cursor_;
    Predicate<IndexFields> *predicate_;
    bool at_end_;

    bool is_positioned_;
    String minkey_;
    String maxkey_;
    fieldlist_type fieldlist_;
};

template <int arity, typename IndexFields>
class TableImpl;

#define ARITY 1
#include "detail/func.hpp"
#undef ARITY

#define ARITY 2
#include "detail/func.hpp"
#undef ARITY

#define ARITY 3
#include "detail/func.hpp"
#undef ARITY

#define ARITY 4
#include "detail/func.hpp"
#undef ARITY

#define ARITY 5
#include "detail/func.hpp"
#undef ARITY

#define ARITY 6
#include "detail/func.hpp"
#undef ARITY

#define ARITY 7
#include "detail/func.hpp"
#undef ARITY

#define ARITY 8
#include "detail/func.hpp"
#undef ARITY

#define ARITY 9
#include "detail/func.hpp"
#undef ARITY

#define ARITY 10
#include "detail/func.hpp"
#undef ARITY

template <typename IndexFields = mpl::vector<String> >
class Table: public TableImpl<mpl::size<IndexFields>::value, IndexFields> {
public:
    Table(const std::string &filename)
        : TableImpl<mpl::size<IndexFields>::value, IndexFields>(filename)
    {}

    FullTableScanIterator begin() {
        return FullTableScanIterator(this->get_bdb_cursor(), false);
    }

    FullTableScanIterator end() {
        return FullTableScanIterator(this->get_bdb_cursor(), true);
    }

    FilterIterator<IndexFields> filtered_begin(
            Predicate<IndexFields> *predicate = NULL) {
        return FilterIterator<IndexFields>(*this, this->get_bdb_cursor(),
                predicate, false);
    }

    FilterIterator<IndexFields> filtered_end() {
        return FilterIterator<IndexFields>(*this, this->get_bdb_cursor(),
                NULL, true);
    }
};

template <typename IndexFields>
int  BdbImpl<IndexFields>::btree_compare_function(Db *db, 
        const Dbt *dbt1, const Dbt *dbt2) 
{
    Table<IndexFields> *table 
        = reinterpret_cast<Table<IndexFields>*>(db->get_app_private());

    String key1(static_cast<char *>(dbt1->get_data()), dbt1->get_size());
    String key2(static_cast<char *>(dbt2->get_data()), dbt2->get_size());

    return table->compare_keys(key1, key2);
}

template <typename IndexFields>
void unpack_key_shim(Table<IndexFields> &table,
    typename Predicate<IndexFields>::fieldlist_type &fieldlist,
    const String &key) 
{ 
    table.unpack_key(fieldlist, key); 
}

template <typename IndexFields>
void bulkinsert(Table<IndexFields> &table) {
    static const char *randarray[10] = {
        "access-log", "redir-log", "blockstat-log", "smile-log", "mil-log",
        "bim-log", "freak-log", "watch-log", "z-log", "x-log"
    };
    for (int i = 0; i < 100000; ++i) {
        const char *f1 = randarray[rand() % 10];
        const char *f4 = randarray[rand() % 10];
        const char *value = randarray[rand() % 10];
        table.insert(mkstring(f1), Int32(rand()),
            Int32(rand()), mkstring(f4), mkstring(value));
    }
}

int main() {


    typedef mpl::vector<String, Int32, Int32, String> myscheme;
    //unlink("logs.db");
    Table<myscheme> tbl("logs.db");
    //bulkinsert(tbl);
    //return 0;

    typedef PredicateBuilder<myscheme>::Arg<0>::type _1;
    typedef PredicateBuilder<myscheme>::Arg<1>::type _2;
    typedef PredicateBuilder<myscheme>::Arg<2>::type _3;
    typedef PredicateBuilder<myscheme>::Arg<3>::type _4;

    PredicateBuilder<myscheme> builder;
    Predicate<myscheme> *predicate
        = new Predicate<myscheme>(builder.evaluate<0>(
                _1() < "z" && _1() > "redir-log"
                && _2() > 100 && _2() < 200));

    String *k1(mknewbufstring(4096));
    Int32 *k2(new Int32());
    Int32 *k3(new Int32());
    String *k4(mknewbufstring(4096));

    FilterIterator<myscheme> fbegin = tbl.filtered_begin(predicate);
    FilterIterator<myscheme> fend = tbl.filtered_end();
    while (fbegin != fend) {
        FilterIterator<myscheme>::value_type v = *fbegin;
        tbl.unpack_key(*k1, *k2, *k3, *k4, v.first);
        std::cout 
            << k1->to_printable_string() << "/" 
            << k2->to_printable_string() << "/"
            << k3->to_printable_string() << "/"
            << k4->to_printable_string() << "  ->   "
            << v.second.to_printable_string() << std::endl;
        ++fbegin;
    }

    delete k4;
    delete k3;
    delete k2;
    delete k1; 

    return 0;
}
