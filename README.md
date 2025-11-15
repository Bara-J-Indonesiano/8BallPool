# Install Raylib
Linux:
    ```
    sudo add-apt-repository ppa:texus/raylib
    sudo apt install libraylib5-dev
    ```

# Compile
compile:
    ```
    g++ billiard_8ball.cpp -o billiard -lraylib -lm -lpthread -ldl -lrt -lGL
    ```

# Run
run:
    ```b
    ./billiard
    ```
