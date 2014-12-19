
all = $(wildcard *.cpp *.h)
sources = $(wildcard *.cpp)
object_files = $(patsubst %.cpp,%.o,$(sources))
libs = -lm
compiler = g++

idlib: $(object_files)
	$(compiler) -shared -o libidlib.so $(object_files) $(libs)

%.o: %.cpp
	$(compiler) -c $<	

install:
	cp libidlib.so /usr/lib/

clean:
	rm -f $(object_files)
