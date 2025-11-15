# Install Raylib
Linux:
    ```bash
    sudo add-apt-repository ppa:texus/raylib
    sudo apt install libraylib5-dev
    ```

# Compile
compile:
    ```bash
    g++ billiard_8ball.cpp -o billiard -lraylib -lm -lpthread -ldl -lrt -lGL
    ```

# Run
run:
    ```bash
    ./billiard
    ```
