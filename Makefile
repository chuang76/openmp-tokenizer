all: run 

run: 
	@g++ tokenizer.cpp -fopenmp -o a.out -std=c++0x
	@./a.out 8 keywords.txt files
