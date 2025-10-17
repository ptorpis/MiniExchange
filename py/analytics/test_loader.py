from py.analytics.loader import load_all_events

for event_type, chunk in load_all_events(chunksize=10000):
    print(f"Loaded chunk from {event_type}: {len(chunk)} rows.")