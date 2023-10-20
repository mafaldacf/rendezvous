FROM mafaldacf/rendezvous-deps:latest

WORKDIR /app
COPY . .

RUN ./rendezvous.sh local clean && ./rendezvous.sh local build