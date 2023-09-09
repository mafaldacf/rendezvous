FROM mafaldacf/rendezvous-deps:latest

WORKDIR /app
COPY . .

RUN pip install -r datastore-monitor/requirements.txt
RUN ./rendezvous.sh local clean && ./rendezvous.sh local build