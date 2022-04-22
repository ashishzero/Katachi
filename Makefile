BUILDDIR := bin
OBJDIR   := bin/objs
TARGET   := Katachi

CPPFLAGS = -std=c++17 -DNETWORK_OPENSSL_ENABLE -I/usr/local/ssl/include
LIBS     = -L/usr/local/ssl/lib64 -lssl -lcrypto

OBJECTS := $(OBJDIR)/KrCommon.o $(OBJDIR)/KrBasic.o $(OBJDIR)/Main.o $(OBJDIR)/Json.o $(OBJDIR)/Network.o $(OBJDIR)/StringBuilder.o

ifndef config
  config=debug
endif

ifeq ($(config),debug)
	CPPFLAGS += -g -Og
endif

ifeq ($(config),release)
	CPPFLAGS += -O2
endif

all: $(BUILDDIR) $(OBJDIR) $(BUILDDIR)/$(TARGET)
	g++ $(CPPFLAGS) $(OBJECTS) -o $(BUILDDIR)/$(TARGET) $(LIBS)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(OBJDIR): $(BUILDDIR)
	mkdir -p $(OBJDIR)

$(BUILDDIR)/$(TARGET): $(OBJECTS)

$(OBJDIR)/KrCommon.o: Kr/KrCommon.cpp
	g++ $(CPPFLAGS) -c Kr/KrCommon.cpp -o $(OBJDIR)/KrCommon.o

$(OBJDIR)/KrBasic.o: Kr/KrBasic.cpp
	g++ $(CPPFLAGS) -c Kr/KrBasic.cpp -o $(OBJDIR)/KrBasic.o

$(OBJDIR)/Main.o: Main.cpp
	g++ $(CPPFLAGS) -c Main.cpp -o $(OBJDIR)/Main.o

$(OBJDIR)/Json.o: Json.cpp
	g++ $(CPPFLAGS) -c Json.cpp -o $(OBJDIR)/Json.o

$(OBJDIR)/Network.o: Network.cpp
	g++ $(CPPFLAGS) -c Network.cpp -o $(OBJDIR)/Network.o

$(OBJDIR)/StringBuilder.o: StringBuilder.cpp
	g++ $(CPPFLAGS) -c StringBuilder.cpp -o $(OBJDIR)/StringBuilder.o

clean:
	rm -r $(BUILDDIR)
