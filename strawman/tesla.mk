.SUFFIXES: .c .dot .ll .pdf .tesla

ANALYSE=	tesla-analyse
INSTRUMENT=	tesla-instrument -S -verify-each
GRAPH=		tesla-graph

CFLAGS+=	-D TESLA
LIBS+=		-l tesla

TESLA_ALLFILES=	.tesla ${TESLA_FILES} ${TESLA_IR} ${TESLA_OBJS}
TESLA_FILES=	${SRCS:.c=.tesla}
TESLA_IR=	${SRCS:.c=.instr.ll} ${IR:.ll=.instr.ll}
TESLA_OBJS=	${TESLA_IR:.ll=.o}

# Replace normal OBJS with TESLA-instrumented versions.
OBJS=		${TESLA_OBJS}

.tesla: ${TESLA_FILES}
	cat $^ > $@
	# temporary workaround for ClangTool irritant:
	sed -i.backup "s@`pwd`/@@" .tesla && rm .tesla.backup

# Run the TESLA analyser over C code.
.c.tesla: tesla
	${ANALYSE} $< -o $@ -- ${CFLAGS} || rm -f $@

# Instrument LLVM IR using TESLA.
%.instr.ll: %.ll .tesla
	${INSTRUMENT} -tesla-manifest .tesla $< -o $@

# Graph the .tesla file.
tesla.dot: .tesla
	${GRAPH} -d -o $@ $<

# Optional PDF-ification of the .dot file.
.dot.pdf:
	dot -Tpdf -o $@ $<

