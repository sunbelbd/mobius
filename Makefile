CXX=g++

all : mobius

mobius : main.cc config.h graph.h parser_dense.h parser.h data.h logger.h
	$(CXX) main.cc -o mobius -std=c++11 -Ofast -march=native -g -flto -funroll-loops -DOMP -fopenmp

mobius_single : main.cc config.h graph.h parser_dense.h parser.h data.h logger.h
	$(CXX) main.cc -o mobius_single -std=c++11 -Ofast -march=native -g -flto -funroll-loops

mobius.so : main.cc config.h graph.h parser_dense.h parser.h data.h logger.h interface.cc
	$(CXX) -shared -fPIC interface.cc -o mobius.so -std=c++11 -Ofast -march=native -g -flto -funroll-loops -DOMP -fopenmp
