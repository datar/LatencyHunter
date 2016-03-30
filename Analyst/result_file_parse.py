import sys
import numpy as np
import pandas as pd
from scipy.stats import scoreatpercentile

RAW_RESULT_FILE_COL_NUMBER = 4
RAW_RESULT_CLIENT_SPLIT_COMMA_INDEX = 3
RAW_RESULT_COL_SEP = ','
MERGED_RESULT_COLS_NUMBER = 16
MERGED_RESULT_COLS_NAME = ['C_SW_TX_SEC',
                           'C_SW_TX_NS',
                           'C_HW_TX_SEC',
                           'C_HW_TX_NS',
                           'S_HW_RX_SEC',
                           'S_HW_RX_NS',
                           'S_SW_RX_SEC',
                           'S_SW_RX_NS',
                           'S_SW_TX_SEC',
                           'S_SW_TX_NS',
                           'S_HW_TX_SEC',
                           'S_HW_TX_NS',
                           'C_HW_RX_SEC',
                           'C_HW_RX_NS',
                           'C_SW_RX_SEC',
                           'C_SW_RX_NS'
                           ]

TIMESTAMP_TABLE_COLS_NAME = ['C_SW_TX', 'C_HW_TX', 'S_HW_RX', 'S_SW_RX', 'S_SW_TX', 'S_HW_TX', 'C_HW_RX', 'C_SW_RX']
LATENCY_TABLE_COLS_NAME = ['C_TX', 'NET_GO', 'S_RX', 'S_IN', 'S_TX', 'NET_BACK', 'C_RX', 'C_IN',
                           'LATENCY', 'LATENCY_NET']
STATS_TABLE_COLS_NAME = ['C_TX', 'NET_GO', 'S_RX', 'S_IN', 'S_TX', 'NET_BACK', 'C_RX', 'C_IN',
                         'LATENCY', 'LATENCY_NET', 'NET_RATIO(%)']
STAT_NAMES = ['MEAN', 'SD', 'MAX', '99.99Tile', '99Tile', '90Tile', 'MEDIAN', 'MIN']


def merge_client_server_file(client_file, server_file):
    client_result = np.genfromtxt(client_file, dtype=int, delimiter=RAW_RESULT_COL_SEP)
    server_result = np.genfromtxt(server_file, dtype=int, delimiter=RAW_RESULT_COL_SEP)
    return np.hstack((client_result[:, :4], server_result, client_result[:, 4:]))


def generate_timestamp_from_raw_data(raw_data, shift=False):
    if shift:
        sec_zero = raw_data[0, 0]
        ns_zero = raw_data[0, 1]
    else:
        sec_zero = 0
        ns_zero = 0
    m, n = raw_data.shape
    if n % 2 != 0:
        raise ValueError("result data col number wrong!")
    ts = (raw_data[:, 0::2]-sec_zero)*1e9+(raw_data[:, 1::2]-ns_zero)
    return ts


def generate_latency_from_timestamp(timestamp):
    latency_step = timestamp[1:, 1:] - timestamp[1:, 0:-1]
    latency_round = (timestamp[1:, -1:] - timestamp[1:, :1])/2
    latency_network = (latency_step[:, 5:6] + latency_step[:, 1:2])/2
    latency_in_client = timestamp[1:, 0:1] - timestamp[0:-1, -1:]
    return np.hstack((latency_step, latency_in_client, latency_round, latency_network))


def test():
    latency = parse_raw_result("C:/Project/client_example.csv", "C:/Project/server_example.csv", "C:/Project/all_latency.csv")
    analyze_latency(latency)


def parse_raw_result(client, server, latency, merged=None, timestamp=None):
    if (client and server and latency) is None:
        raise ValueError("Missing filename to parse!")
    merged_data = merge_client_server_file(client, server)
    if merged:
        np.savetxt(merged, merged_data,
                   fmt='%d',
                   delimiter=RAW_RESULT_COL_SEP,
                   header=RAW_RESULT_COL_SEP.join(MERGED_RESULT_COLS_NAME))
    ts = generate_timestamp_from_raw_data(merged_data, shift=True)
    if timestamp:
        np.savetxt(timestamp, ts,
                   fmt='%d',
                   delimiter=RAW_RESULT_COL_SEP,
                   header=RAW_RESULT_COL_SEP.join(TIMESTAMP_TABLE_COLS_NAME))
    latency_data = generate_latency_from_timestamp(ts)
    np.savetxt(latency, latency_data,
               fmt='%d',
               delimiter=RAW_RESULT_COL_SEP,
               header=RAW_RESULT_COL_SEP.join(LATENCY_TABLE_COLS_NAME))
    return latency_data


def analyze_latency(latency):
    network_ratio = 100*latency[:, -1:] / latency[:, -2:-1]
    data = np.hstack((latency, network_ratio))
    median = np.percentile(data, 50, 0)
    tile9 = np.percentile(data, 90, 0)
    tile99 = np.percentile(data, 99, 0)
    tile9999 = scoreatpercentile(data, 99.99, axis=0)
    average = np.mean(data, 0)
    sd = np.std(data, 0)
    max_value = np.max(data, 0)
    min_value = np.min(data, 0)
    stat = np.vstack((average, sd, max_value, tile9999, tile99, tile9, median, min_value)).astype(int)
    return stat


def main():
    if len(sys.argv) < 5:
        raise AssertionError("this_program client_file server_file latency_file stat_file!")
    client_file = sys.argv[1]
    server_file = sys.argv[2]
    latency_file = sys.argv[3]
    stats_file = sys.argv[4]
    latency_file = parse_raw_result(client_file, server_file, latency_file)
    stat = analyze_latency(latency_file)
    df = pd.DataFrame(stat, index=STAT_NAMES, columns=STATS_TABLE_COLS_NAME)
    df.to_csv(stats_file, index=True, header=True, sep=RAW_RESULT_COL_SEP)

if __name__ == '__main__':
    main()
