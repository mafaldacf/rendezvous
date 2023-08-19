FROM mafaldacf/rendezvous-deps:latest

WORKDIR /app
COPY . .

RUN pip install -r datastore-monitor/requirements.txt
RUN ./manager.sh local clean && ./manager.sh local build