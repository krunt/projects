
all = $(wildcard *.cpp *.h)
sources = $(wildcard *.cpp opengl/*.cpp)
headers = $(wildcard *.h opengl/*.h)
object_files = $(patsubst %.cpp,%.o,$(sources))
libs = -lm -Ld3lib -lidlib -lglut -lGL -lSDL -lGLEW -lGLU
flags = -g -O0
compiler = g++

box: $(object_files)
	$(compiler) $(flags) -o box $(object_files) $(libs)

%.o: %.cpp $(headers)
	$(compiler) $(flags) -c $< -std=c++11

clean:
	rm -f $(object_files); rm box
