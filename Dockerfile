FROM mafaldacf/rendezvous-deps:latest

WORKDIR /app
COPY . .

RUN cd client-process/python ;\
    pip install -r requirements.txt

RUN cd rendezvous-server ;\
    ./start.sh clean ;\
    ./start.sh build