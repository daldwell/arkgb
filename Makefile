SRC_PATH = src
OBJ_PATH = obj
OBJS = $(patsubst $(SRC_PATH)/%.cpp, $(OBJ_PATH)/%.o,$(wildcard $(SRC_PATH)/*.cpp))
VPATH = $(SRC_PATH)
INC_PATH = -IC:\mingwlib\SDL2-2.0.20\x86_64-w64-mingw32\include\SDL2
LIB_PATH = -LC:\mingwlib\SDL2-2.0.20\x86_64-w64-mingw32\lib
CFLAGS = -c -w -Wl,-subsystem,windows
LFLAGS = -lmingw32 -lSDL2main  -lSDL2 
CC = x86_64-w64-mingw32-g++

ArkGB.exe: $(OBJS)
	$(CC) $(OBJS) $(INC_PATH) $(LIB_PATH) $(LFLAGS)  -o $@

$(OBJ_PATH)/%.o: %.cpp
	$(CC) $(INC_PATH) $(CFLAGS) -o $@ $<


ArkGB.o: ArkConsole.h
ConsoleIO.o: ConsoleIO.h
ArkConsole.o: ConsoleIO.h

.PHONY : clean
clean:
	rm ArkGB.exe $(OBJS)
