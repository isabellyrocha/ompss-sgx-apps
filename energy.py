from influxdb import InfluxDBClient
import numpy

def query_pdu_data(node_name, start, end):
    client = InfluxDBClient('localhost', 8086, 'root', 'root', 'energy')
    result = client.query('SELECT value '
                'FROM "pdu_power/node_utilization" '
                'WHERE nodename =~ /%s/ AND '
                '%s000000000 <= time AND '
                'time <= %s000000000 ' % (node_name, start, end))
#    print(result.get_points(measurement='pdu_power/node_utilization')[0])
    return list(result.get_points(measurement='pdu_power/node_utilization'))

def pdu_energy(start, end):
    energy = 0
    #for node_name in ['eiger-7']:
    points = query_pdu_data('eiger-6', start, end)
#    print(points)
    values = []

    #last = 0
    for i in range(len(points)):
 #       print(points)
        value = points[i]['value']
            #print(value)
            #if not value == None:
            #    for j in range(last,i):
        if not value == None:
            values.append(value)
             #   last = i
        #for i in range(last, len(points)):
        #    values.append(value)
#        print(values)
        if len(values) == 1:
            energy += values[0]
        else:
            energy += (numpy.trapz(values))
    return energy

for application in ["cholesky", "dot-product", "matmul", "stream-barr", "stream-deps"]:
    with open("%s/exp.out" % application) as f:
        line = f.readline()
        while line:
#           print(line)
            version, exp, it, input_size, block_size, threads, scheduler, start, end, duration = line.split(",")
            print("%s,%s,%s" % (application, line.strip(), pdu_energy(start, end)))
            line = f.readline()
