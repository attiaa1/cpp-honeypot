FROM ubuntu:latest

WORKDIR /app

RUN apt-get update && apt-get install -y \
    g++ \
    clang

COPY honeypot.cpp .

RUN clang++ -o honeypot honeypot.cpp 

EXPOSE 2222

CMD ["./honeypot"]
