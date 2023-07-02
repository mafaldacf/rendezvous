FROM mafaldacf/rendezvous-deps:latest

WORKDIR /app
COPY . .

RUN cd subscriber-process ;\
    pip install -r requirements.txt

RUN cd client-eval ;\
    pip install -r requirements.txt

RUN cd rendezvous-server ;\
    ./rendezvous.sh clean ;\
    ./rendezvous.sh build