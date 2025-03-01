g++ -std=c++23 -O2 -c main.cpp -o arw-preview.o && g++ -std=c++23 -O2 arw-preview.o -o arw-preview -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio -lsfml-network
#g++ -std=c++23 -O2 -c -fsanitize=thread main.cpp -o arw-preview.o && g++ -std=c++23 -O2 -fsanitize=thread arw-preview.o -o arw-preview -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio -lsfml-network

# -g debug flag
#g++ -std=c++23 -O2 -g -c main.cpp -o arw-preview.o && g++ -std=c++23 -O2 -g arw-preview.o -o arw-preview -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio -lsfml-network

# -g debug flag, -O0
#g++ -std=c++23 -O0 -g -c main.cpp -o arw-preview.o && g++ -std=c++23 -O0 -g arw-preview.o -o arw-preview -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio -lsfml-network
