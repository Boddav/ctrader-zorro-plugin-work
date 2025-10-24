# get full timeseries data
import datetime as dt
import json
import os
from timeit import default_timer as timer
import pandas as pd
CHUNK_SIZE = {'days': 1} # span covered in 1 request
THRESHOLD_OF_EMPTY_RESPONSES = 60 # cutoff logic
outfile = 'bars.raw.csv'
statefile = 'last_fromTimestamp.txt'
cols = ['utcTimestampInMinutes','low','deltaOpen','deltaClose','deltaHigh','volume']
request_timer = 0 # for measuring time taken by requests
empty_resp_counter = 0 # for counting sequantial empty responses
def main():
  sym_id = 41 # 'XAUUSD'
  start, end = None, None
  if os.path.exists(statefile):
    with open(statefile) as f:
      last_saved_fromTimestamp = f.read()
    last_fromDatetime = dt.datetime.fromtimestamp(int(last_saved_fromTimestamp)/1000)
    # date range must not be redundant between runs (otherwise creates duplicates in outfile)
    start = last_fromDatetime - dt.timedelta(**CHUNK_SIZE)
    end = last_fromDatetime
  else:
    now = dt.datetime.now(tz=dt.UTC)
    today_midnight = now.replace(hour=0, minute=0, second=0, microsecond=0)
    start = today_midnight - dt.timedelta(**CHUNK_SIZE)
    end = today_midnight
  fr, to = [int(i.timestamp()*1000) for i in [start, end]]
  reqBars(sym_id, fr, to)
  print('FROM:                           TO:                             SECS_TOOK:      BARS:')

def reqBars(sym_id, fr, to):
  req = OA.ProtoOAGetTrendbarsReq()
  req.symbolId = sym_id
  req.ctidTraderAccountId = credentials['accountId']
  req.period = OAModel.ProtoOATrendbarPeriod.M1
  req.fromTimestamp = fr
  req.toTimestamp = to
  deferred = client.send(req, responseTimeoutInSeconds=20)
  deferred.addCallbacks(onTrendbar, onError, [fr, to])
  global request_timer
  request_timer = timer()

def onTrendbar(message, begin, end):
  response = Protobuf.extract(message)
  # print some stuff about the chunk
  global request_timer
  chunk_info = [
    *[dt.datetime.fromtimestamp(i/1000) for i in [begin,end]],
    round((timer()-request_timer)),
    len(response.trendbar),
  ]
  print('\t\t'.join(map(str,chunk_info)))
  if message.payloadType == OA.ProtoOAErrorRes().payloadType:
    print('server sent error')
  # process chunk response
  if len(response.trendbar) > 0:
    chunk_bars = [[str(getattr(i,k)) for k in cols] for i in response.trendbar]
    chunk_str = '\n'.join([','.join(i) for i in chunk_bars]) + '\n'
    with open(outfile, 'a', newline='') as f: f.write(chunk_str)
  # update state file
  with open(statefile, 'w') as f: f.write(str(begin))
  # count up empty server responses (used to detect when reached end of data)
  global empty_resp_counter
  empty_resp_counter = 0 if len(response.trendbar) > 0 else (empty_resp_counter + 1)
  # assume we reached end of data if last n requests had no data
  if empty_resp_counter > THRESHOLD_OF_EMPTY_RESPONSES:
    df = pd.read_csv(outfile, names=cols)
    df2 = pd.DataFrame()
    df2['timestamp'] = df['utcTimestampInMinutes'] * 60
    df2['open'] = (df['low'] + df['deltaOpen']) / 100_000
    df2['high'] = (df['low'] + df['deltaHigh']) / 100_000
    df2['low'] = df['low'] / 100_000
    df2['close'] = (df['low'] + df['deltaClose']) / 100_000
    df2['volume'] = df['volume']
    df2.sort_values(by='timestamp', ascending=True, inplace=True)
    df2.to_csv('bars.csv', index=False)
    print('all done. shutting down...')
    reactor.stop()
    return
  # request next chunk
  prev_frm = dt.datetime.fromtimestamp(begin / 1000)
  new_frm = prev_frm - dt.timedelta(**CHUNK_SIZE)
  fr, to = [int(i.timestamp()*1000) for i in [new_frm, prev_frm]]
  reqBars(response.symbolId, fr, to)
