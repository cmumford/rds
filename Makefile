
CLANG_FORMAT=clang-format

SOURCE_FILES = \
	  include/rds_decoder.h \
	  include/mgos_rds_decoder.h \
		src/freq_table.c \
		src/freq_table.h \
		src/freq_table_group.c \
		src/freq_table_group.h \
		src/mgos_rds_decoder.c \
		src/rds_decoder.c

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
