all:
	mkdir -p output
	gcc -std=c99 -Wall -Werror -Iinclude -c src/c_compiler/main.c -o output/main.o
	gcc -std=c99 -Wall -Werror -Iinclude -c src/c_compiler/lexer.c -o output/lexer.o
	gcc -std=c99 -Wall -Werror -Iinclude -c src/c_compiler/parser.c -o output/parser.o
	gcc -std=c99 -Wall -Werror -Iinclude -c src/c_compiler/serializer.c -o output/serializer.o
	gcc -std=c99 -Wall -Werror -Iinclude -c src/c_compiler/expand.c -o output/expand.o
	gcc output/main.o output/lexer.o output/parser.o output/serializer.o output/expand.o -o output/latex_compiler

clean:
	rm -rf output parser_output.md