
all = $(wildcard *.cpp *.h)
sources = $(wildcard *.cpp)
object_files = $(patsubst %.cpp,%.o,$(sources))
libs = -lm
compiler = g++
flags = -O2

idlib: $(object_files)
	$(compiler) $(flags) -shared -o libidlib.so $(object_files) $(libs)

%.o: %.cpp
	$(compiler) $(flags) -c $<	

install:
	cp libidlib.so /usr/lib/

clean:
	rm -f $(object_files)
