FROM alpine:3.20
RUN apk add --no-cache python3 tcpdump
WORKDIR /app
COPY entrypoint.sh capture.sh mock_v2x.py mock_ivi.py ./
CMD ["./entrypoint.sh"]
