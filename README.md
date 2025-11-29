# Install Raylib
Linux:
    ```
    sudo add-apt-repository ppa:texus/raylib
    sudo apt install libraylib5-dev
    ```

# Compile
compile:
Windows MSYS 2:
```
g++ billiard_8ball.cpp -o billiard.exe -lraylib -lopengl32 -lgdi32 -lwinmm
```

Ubuntu/Debian/Mint:
```
g++ billiard_8ball.cpp -o billiard -lraylib -lm -ldl -lpthread -lGL
```

Arch Linux/Manjaro:
```
g++ billiard_8ball.cpp -o billiard -lraylib -lm
```

# Run
run:
    ```
    ./billiard
    ```
