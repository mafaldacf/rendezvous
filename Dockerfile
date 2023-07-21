FROM mafaldacf/rendezvous-deps:latest

WORKDIR /app
COPY . .

RUN cd datastore-monitor ;\
    pip install -r requirements.txt

RUN cd client-eval ;\
    pip install -r requirements.txt

RUN ./manager.sh local clean ;\
    ./manager.sh local build