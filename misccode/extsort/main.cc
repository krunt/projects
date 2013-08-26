#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

/* sorting parameters */
enum { 
       MAX_SIZE = 128 * 4 * 1024, 
       MAX_LINE_SIZE = 4 * 1024,
       MERGE_BUFFPEKS = 7, 
};

const char *buffpeks_filename = "buffpeks";

typedef int64_t i64;
char *read_i64(char *p, i64 *x) {
    *x = *(i64*)p;
    return p + 8;
}
char *write_i64(char *p, i64 x) {
    *(i64*)p = x;
    return p + 4;
}

class Buffpek {
public:

public:
    Buffpek() {}

    Buffpek(int fpos, int size)
        : fpos_(fpos), size_(size)
    {}

    static int pack_size() {
        return 8 + 8;
    }

    void serialize(char *buf) {
        write_i64(buf, (i64)fpos_);
        buf += 8;
        write_i64(buf, (i64)size_);
    }

    void deserialize(char *buf) {
        read_i64(buf, &fpos_);
        buf += 8;
        read_i64(buf, &size_);
    }
    
    int fpos() const {
        return fpos_;
    }

    int size() const {
        return size_;
    }

private:
    i64 fpos_;
    i64 size_;
};

class FastString {
public:
    FastString()
        : str_(NULL), len_(0)
    {}
    FastString(char *p, int sz)
        : str_(p), len_(sz)
    {}
    char *str() const { return str_; }
    int len() const { return len_; }
    bool operator<(const FastString &s) const {
        return memcmp(str_, s.str_, std::min(len_, s.len_)) < 0;
    }
private:
    char *str_;
    int len_;
};

class MergeElement {
public:
    MergeElement(FILE *fd, const Buffpek &buffpek) 
        : fd_(fd), initial_fpos(buffpek.fpos()), 
        fpos(buffpek.fpos()), fsize(buffpek.size())
    {
        fseek(fd_, 0, SEEK_END);
        fend = ftell(fd_);
        assert(readnext());
    }

    bool readnext() {
        fseek(fd_, fpos, SEEK_SET);
        if (fpos >= fend || fpos >= initial_fpos + fsize)
            return false;
        if (!fgets(line, MAX_LINE_SIZE, fd_))
            return false;
        cached_string_ = FastString(line, strlen(line));
        fpos += strlen(line);
        return true;
    }

    const FastString &str() const {
        return cached_string_;
    }

    /* reverse sotring */
    bool operator<(const MergeElement &elm) const {
        return !(cached_string_ < elm.cached_string_);
    }

private:  
    FILE *fd_;
    int initial_fpos;
    int fpos;
    int fsize;
    int fend;
    char line[MAX_LINE_SIZE];
    FastString cached_string_;
};

class HeapQueue {
public:
    HeapQueue()
    {}

    bool empty() const {
        return !elements.size();
    }

    int size() const {
        return elements.size();
    }

    MergeElement &top() {
        return elements.front();
    }

    void push(const MergeElement &elm) {
        elements.push_back(elm);
        std::push_heap(elements.begin(), elements.end());
    }

    void pop() {
        std::pop_heap(elements.begin(), elements.end());
        elements.pop_back();
    }

private:
    std::vector<MergeElement> elements;
};

class ExtSorter {
public:
    ExtSorter(const std::string &infile, const std::string &outfile)
        : tmpfname1_("x"), tmpfname2_("y")
    {
        infile_ = fopen(infile.c_str(), "rb");
        outfile_ = fopen(outfile.c_str(), "wb");
        tmpfile1_ = fopen(tmpfname1_, "wb");
        tmpfile2_ = fopen(tmpfname2_, "wb");
        buffpeks_ = fopen(buffpeks_filename, "wb");

        buffpek_buf = (char*)malloc(Buffpek::pack_size());
        buffpek_databuf = (char*)malloc(MAX_SIZE);
    }

    ~ExtSorter() {
        free(buffpek_buf);
        free(buffpek_databuf);
        fclose(buffpeks_);
        fclose(infile_);
        fclose(outfile_);
    }

    bool operator()();

private:
    bool split_infile_to_buffpeks();
    bool flush_buffpeks_strings(std::vector<FastString> &buffpek_strings,
        int size);
    bool read_all_buffpeks(std::vector<Buffpek> &bpks);
    bool merge_many_buffers();
    bool merge_final_buffers() { return true; }

private:
    FILE *infile_;
    FILE *outfile_;

    FILE *tmpfile1_;
    FILE *tmpfile2_;

    FILE *buffpeks_;

    char *buffpek_buf;
    char *buffpek_databuf;

    const char *tmpfname1_; 
    const char *tmpfname2_; 
};


bool
ExtSorter::flush_buffpeks_strings(
    std::vector<FastString> &buffpek_strings,
    int size)
{
    Buffpek bf(ftell(tmpfile1_), size);
    bf.serialize(buffpek_buf);
    fwrite(buffpek_buf, 1, Buffpek::pack_size(), buffpeks_);

    std::sort(buffpek_strings.begin(), buffpek_strings.end());
    for (int i = 0; i < buffpek_strings.size(); ++i) {
        fwrite(buffpek_strings[i].str(), 1, buffpek_strings[i].len(), tmpfile1_);
    }
    return true;
}

bool 
ExtSorter::split_infile_to_buffpeks() {
    char line[MAX_LINE_SIZE];
    char *p, *buffpek_databuf_pointer;

    buffpek_databuf_pointer = buffpek_databuf;
    std::vector<FastString> buffpek_strings;
    while (fgets(line, sizeof(line), infile_)) {
        int len = strlen(line);
        if (buffpek_databuf_pointer - buffpek_databuf + len > MAX_SIZE)
        {
            flush_buffpeks_strings(buffpek_strings,
                buffpek_databuf_pointer - buffpek_databuf);
            buffpek_strings.clear();
            buffpek_databuf_pointer = buffpek_databuf;
        }
        memcpy(buffpek_databuf_pointer, line, len);
        buffpek_strings.push_back(
            FastString(buffpek_databuf_pointer, len));
        buffpek_databuf_pointer += len;
    }
    if (buffpek_databuf_pointer > buffpek_databuf) {
        flush_buffpeks_strings(buffpek_strings,
            buffpek_databuf_pointer - buffpek_databuf);
    }
    fflush(buffpeks_);
    return true;
}

bool
ExtSorter::read_all_buffpeks(std::vector<Buffpek> &bfpks) {
    fclose(buffpeks_);
    if (!(buffpeks_ = fopen(buffpeks_filename, "r")))
        return false;
    while (fread(buffpek_buf, 1, Buffpek::pack_size(), buffpeks_) 
            == Buffpek::pack_size()) 
    {
        Buffpek stub;
        stub.deserialize(buffpek_buf);
        bfpks.push_back(stub);
    }
    return true;
}

bool
ExtSorter::merge_many_buffers() {
    FILE *fd1, *fd2;
    const char *fd1fname, *fd2fname;
    std::vector<Buffpek> bfpks;
    read_all_buffpeks(bfpks);

    fd1fname = tmpfname1_;
    fd2fname = tmpfname2_;

    fd1 = tmpfile1_;
    fd2 = tmpfile2_;
    while (bfpks.size() > 1) {
        fclose(fd1); 
        fclose(fd2);
        if (!(fd1 = fopen(fd1fname, "rb")))
            return false;

        if (bfpks.size() <= MERGE_BUFFPEKS) {
            fd2 = outfile_;
        } else {
            if (!(fd2 = fopen(fd2fname, "wb")))
                return false;
        }

        std::vector<Buffpek> newbfpks;
        for (int i = 0; i < bfpks.size(); i += MERGE_BUFFPEKS) {
            HeapQueue hp;
            int total_size = 0;
            int initial_pos = ftell(fd2);
            for (int j = i;
                   j < std::min(i + MERGE_BUFFPEKS, (int)bfpks.size()); ++j) 
            {
                hp.push(MergeElement(fd1, bfpks[j]));
                total_size += bfpks[j].size();
            }
            int t1 = 0;
            while (!hp.empty()) {
                MergeElement &m = hp.top();
                fwrite(m.str().str(), 1, m.str().len(), fd2);
                t1 += m.str().len();
                if (!m.readnext()) {
                    hp.pop();
                }
            }
            std::cout << total_size << " " << t1 << std::endl;
            assert(total_size == t1);
            newbfpks.push_back(Buffpek(initial_pos, total_size));
        }
        bfpks.swap(newbfpks);
        std::swap(fd1fname, fd2fname);
    }
    return true;
}

bool ExtSorter::operator()() {
    if (!split_infile_to_buffpeks())
        return false;
    if (!merge_many_buffers())
        return false;
    if (!merge_final_buffers())
        return false;
    return true;
}

int main(int argc, char **argv) {
    ExtSorter sorter(argv[1], argv[2]); 
    sorter();
    return 0;
}

