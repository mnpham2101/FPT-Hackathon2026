FROM alpine:3.20
RUN apk add --no-cache python3 tcpdump
WORKDIR /app
COPY netcheck.py capture.sh entrypoint.sh ./
RUN chmod +x capture.sh entrypoint.sh
CMD ["./entrypoint.sh"]
