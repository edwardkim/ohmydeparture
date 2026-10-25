ifeq ($(shell uname -s),Haiku)
# Builds R Departures on Haiku. The code is plain C++98, so the default g++
# builds it on an x86_gcc2 hybrid as well.
NAME     = RDepartures
CXX     ?= g++
CXXFLAGS = -O2 -Wall -Wno-multichar
LIBS     = -lbe -ltranslation
SRCS     = src/App.cpp src/Board.cpp src/Data.cpp src/Station.cpp

APPS  = $(HOME)/config/non-packaged/apps
FONTS = $(HOME)/config/non-packaged/data/fonts/otfonts

$(NAME): $(SRCS) src/Board.h src/Data.h src/Station.h src/app.rdef
	$(CXX) $(CXXFLAGS) -o $(NAME) $(SRCS) $(LIBS)
	rc -o $(NAME).rsrc src/app.rdef
	xres -o $(NAME) $(NAME).rsrc
	mimeset -f $(NAME)

install: $(NAME)
	mkdir -p "$(APPS)" "$(FONTS)"
	cp $(NAME) "$(APPS)/R Departures"
	cp fonts/texgyreheroscn-bold.otf "$(FONTS)/RDepartures-texgyreheroscn-bold.otf"

clean:
	rm -f $(NAME) $(NAME).rsrc

.PHONY: install clean
else
include platform/qt/Makefile
endif
