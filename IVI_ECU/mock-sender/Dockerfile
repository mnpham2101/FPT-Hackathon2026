FROM python:3.11-slim

WORKDIR /app

COPY mock_r4_sender.py .

# No additional dependencies needed — pure stdlib (socket, json, os, time)
CMD ["python", "mock_r4_sender.py"]
