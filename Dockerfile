FROM mafaldacf/rendezvous-deps:latest

WORKDIR /app
COPY . .

RUN ./manager.sh local clean ;\
    ./manager.sh local build

RUN cd datastore-monitor ;\
    pip install -r requirements.txt

RUN cd client-eval ;\
    pip install -r requirements.txt