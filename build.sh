#rm -Wall

mainRoutineSourceFile=main.c
ProgramName=cwin
rm $ProgramName

gcc $mainRoutineSourceFile -g -o $ProgramName -lncurses -lpthread