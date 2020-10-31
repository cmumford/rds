
CLANG_FORMAT=clang-format

SOURCE_FILES = \
	  include/*.h \
		src/*.[ch] \
		util/*.cc \
		util/*.h

.PHONY: format
format:
	${CLANG_FORMAT} -i ${SOURCE_FILES}

docs: doxygen.conf Makefile
	doxygen doxygen.conf

.PHONY: clean
clean:
	rm -rf build docs

.PHONY: tags
tags:
	ctags --extra=+f --languages=+C,+C++ --recurse=yes --links=no
