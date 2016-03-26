

def merge_client_server_result(client, server):
    if len(client) != len(server):
        return None
    line_number = len(client)
    result = []
    for i in  range(line_number):
        client[i].find()
    return result


def merge_client_server_file(client_file, server_file, out_filename = None):
    with open(client_file) as client:
        client_result = client.readlines()
    with open(server_file) as server:
        server_result = server.readlines()
    result = merge_client_server_result(client_result, server_result)
    if out_filename:
        outfile = open(out_filename, 'w')
        outfile.write(result)
        outfile.close()
    return result


