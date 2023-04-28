FROM leafen00/rendezvous-deps:latest

#ENV REPLICA_ID="eu-central-1"

WORKDIR /app
COPY . .

RUN ./start.sh clean
RUN ./start.sh build
#CMD ["./start.sh", "run", "server", $REPLICA_ID]