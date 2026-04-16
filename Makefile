# Variables
CC = gcc
AR = ar
CFLAGS = -Wvla -Wextra -Werror -D_GNU_SOURCE
PICFLAGS =
LDFLAGS = -shared
LIBPATHUFFC = src/ufficio.c #path to input .c file of library ufficio.h
LIBPATHUFFH = include/ufficio.h #path to ufficio.h
LIBPATHOBJUFF = obj/ufficio.o
LIB_UFFICIO = lib/libufficio.so
LIBPATHDIRC = src/Direttore_op.c
LIBPATHDIRH = include/Direttore_op.h
LIBPATHOBJDIR = obj/Direttore_op.o
LIB_DIRETTORE = lib/libDirettore_op.a
LIBPATHUSRC = src/utente_op.c
LIBPATHUSRH = include/utente_op.h
LIBPATHOBJUSR = obj/utente_op.o
LIB_UTENTE = lib/libutente_op.a
DIRPATHC = src/Direttore.c
DIRPATHOBJ = obj/Direttore.o
DIRPATHEXEC = bin/Direttore
EROGPATHC = src/erogatore_ticket.c
EROGPATHOBJ = obj/erogatore_ticket.o
EROGPATHEXEC = bin/erogatore
USRPATHC = src/utente.c
USRPATHOBJ = obj/utente.o
USRPATHEXEC = bin/utente
OPERPATHC = src/operatori.c
OPERPATHOBJ = obj/operatori.o
OPERPATHEXEC = bin/operatore
ADDPATHC = src/add_users.c
ADDOBJ = obj/add_users.o
ADDPATHEXEC = bin/add_users

# Aggiungo i file header comuni
HEADER_UFFICIO = include/ufficio.h
HEADER_MSG = include/msg.h
HEADER_DIR_OP = include/Direttore_op.h
HEADER_USR_OP = include/utente_op.h
HEADER_EROG = include/erogatore_ticket.h
HEADER_OPER = include/operatori.h


.PHONY: all clean directories

#default rule
all : directories makeLibs _CompileAll

makeLibs : $(LIB_UFFICIO) $(LIB_DIRETTORE) $(LIB_UTENTE) | directories

_CompileAll : $(DIRPATHEXEC) $(EROGPATHEXEC) $(USRPATHEXEC) $(OPERPATHEXEC) $(ADDPATHEXEC)

# Correzione: Aggiunte dipendenze alle librerie
$(DIRPATHEXEC) : $(DIRPATHOBJ) $(LIB_DIRETTORE) $(LIB_UFFICIO)
	$(CC) $(CFLAGS) $< -Llib $(LIB_DIRETTORE) $(LIB_UFFICIO) -o $@

# Correzione: Aggiunte dipendenze ai file .h
$(DIRPATHOBJ) : $(DIRPATHC) $(HEADER_DIR_OP) $(HEADER_UFFICIO)
	$(CC) $(CFLAGS) -c $< -o $@

# Correzione: Aggiunte dipendenze alle librerie
$(EROGPATHEXEC) : $(EROGPATHOBJ) $(LIB_DIRETTORE) $(LIB_UFFICIO)
	$(CC) $(CFLAGS) $< -Llib $(LIB_DIRETTORE) $(LIB_UFFICIO) -o $@

# Correzione: Aggiunte dipendenze ai file .h
$(EROGPATHOBJ) : $(EROGPATHC) $(HEADER_UFFICIO) $(HEADER_EROG) $(HEADER_MSG)
	$(CC) $(CFLAGS) -c $< -o $@

# Correzione: Aggiunte dipendenze alle librerie
$(USRPATHEXEC) : $(USRPATHOBJ) $(LIB_UTENTE)
	$(CC) $(CFLAGS) $< -Llib $(LIB_UTENTE) -o $@

# Correzione: Aggiunte dipendenze ai file .h
$(USRPATHOBJ) : $(USRPATHC) $(HEADER_MSG) $(HEADER_USR_OP)
	$(CC) $(CFLAGS) -c $< -o $@

# Correzione: Aggiunte dipendenze alle librerie
$(OPERPATHEXEC) : $(OPERPATHOBJ) $(LIB_UTENTE) $(LIB_UFFICIO)
	$(CC) $(CFLAGS) $< -Llib $(LIB_UTENTE) $(LIB_UFFICIO) -o $@

# Correzione: Aggiunte dipendenze ai file .h
$(OPERPATHOBJ) : $(OPERPATHC) $(HEADER_OPER) $(HEADER_MSG) $(HEADER_UFFICIO)
	$(CC) $(CFLAGS) -c $< -o $@

$(ADDPATHEXEC) : $(ADDOBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(ADDOBJ) : $(ADDPATHC)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_UFFICIO) : $(LIBPATHOBJUFF)
	$(CC) $(LDFLAGS) -o $@ $^

$(LIBPATHOBJUFF) : $(LIBPATHUFFC) $(LIBPATHUFFH)
	$(CC) $(PICFLAGS) -c $< -o $@

$(LIB_DIRETTORE) : $(LIBPATHOBJDIR)
	$(AR) rcs $@ $^

# Correzione: Aggiunte dipendenze ai file .h
$(LIBPATHOBJDIR) : $(LIBPATHDIRC) $(LIBPATHDIRH) $(HEADER_UFFICIO)
	$(CC) $(CFLAGS) -c $(LIBPATHDIRC) -o $@

$(LIB_UTENTE) : $(LIBPATHOBJUSR)
	$(AR) rcs $@ $^

# Correzione: Aggiunte dipendenze ai file .h
$(LIBPATHOBJUSR) : $(LIBPATHUSRC) $(LIBPATHUSRH) $(HEADER_MSG)
	$(CC) $(CFLAGS) -c $(LIBPATHUSRC) -o $@
			
#Directory creation, if necessary
directories:
	mkdir -p lib obj bin tmp stats

clean:
		rm -rf lib obj bin tmp stats
