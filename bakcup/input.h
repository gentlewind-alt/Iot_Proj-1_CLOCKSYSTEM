#ifndef INPUT_H
#define INPUT_H

struct InputState {
    int rotaryDirection; // -1, 0, or 1
    bool swPressed;
    bool rstPressed;
};

#endif
