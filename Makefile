#
# TCP Proxy Server
# ~~~~~~~~~~~~~~~~~~~
#
# Copyright (c) 2007 Arash Partow (http://www.partow.net)
# URL: http://www.partow.net/programming/tcpproxy/index.html
#
# Distributed under the Boost Software License, Version 1.0.
#


COMPILER         = -c++
OPTIMIZATION_OPT = -O3
OPTIONS          = -pedantic -ansi -Wall -Werror $(OPTIMIZATION_OPT) -o
PTHREAD          = -lpthread
LINKER_OPT       = -L/usr/lib -lstdc++ $(PTHREAD) -lboost_thread-mt -lboost_system -lboost_program_options

BUILD_LIST+=smtpServer

all: $(BUILD_LIST)

smtpServer: smtpServer.cpp
	$(COMPILER) $(OPTIONS) ss4lad smtpServer.cpp notifier.cpp $(LINKER_OPT)

strip_bin :
	strip -s smtpServer

clean:
	rm -f core *.o *.bak *~ *stackdump *#
