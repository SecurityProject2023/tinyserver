g++ -fno-stack-protector -z execstack -D_FORTIFY_SOURCE=0 -no-pie -o main main.cc