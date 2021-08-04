import mcasapi
import mcas
import sys
import flatbuffers # you will need to install flatbuffers package for python
import Proto.Message # flat buffer protocol
import Proto.UpdateRequest
import Proto.UpdateReply
import Proto.QueryRequest
import Proto.QueryReply
import Proto.Element


class Tabulator:
    def __init__(self, ip, port):
        self.session = mcas.Session(ip=ip, port=port)

    def __del__(self):
        self.pool.close()

    def create_pool (self, pool_name, pool_size): 
        self.pool = self.session.create_pool(pool_name, pool_size, 100)

    def add_sample(self, key, sample):
        if not(isinstance(sample, float)) or not(isinstance(key, str)):
            raise TypeError
        builder = flatbuffers.Builder(128)

        Proto.UpdateRequest.UpdateRequestStart(builder)
        Proto.UpdateRequest.UpdateRequestAddSample(builder, sample)
        element = Proto.UpdateRequest.UpdateRequestEnd(builder)

        Proto.Message.MessageStart(builder)
        Proto.Message.MessageAddElementType(builder,Proto.Element.Element().UpdateRequest)
        Proto.Message.MessageAddElement(builder, element)
        request = Proto.Message.MessageEnd(builder)
        builder.Finish(request)

        msg = builder.Output() # msg is bytearray
        response = self.pool.invoke_ado(key,
                                        command=msg,
                                        ondemand_size=int(1e5), 
                                        flags=mcasapi.AdoFlags.ZERO_NEW_VALUE.value) # first hit will clear the memory
        return Proto.UpdateReply.UpdateReply.GetRootAsUpdateReply(response, 0)

    def query(self, key):
        if not(isinstance(key, str)):
            raise TypeError

        builder = flatbuffers.Builder(128)
        
        Proto.QueryRequest.QueryRequestStart(builder)
        element = Proto.QueryRequest.QueryRequestEnd(builder)

        Proto.Message.MessageStart(builder)
        Proto.Message.MessageAddElementType(builder,Proto.Element.Element().QueryRequest)
        Proto.Message.MessageAddElement(builder, element)
        request = Proto.Message.MessageEnd(builder)
        builder.Finish(request)

        msg = builder.Output() # msg is bytearray
        response = self.pool.invoke_ado(key,
                                        command=msg,
                                        ondemand_size=16, # PMDK TOID is 16 bytes
                                        flags=mcasapi.AdoFlags.ZERO_NEW_VALUE.value) # first hit will clear the memory

        msg = Proto.Message.Message.GetRootAsMessage(response, 0)
        if msg.ElementType() == Proto.Element.Element().UpdateReply:
            reply = Proto.QueryReply.QueryReply()
            reply.Init(msg.Element().Bytes, msg.Element().Pos)
            return reply
        else:
            PyRETURN_NONE

    def print_query(self, key):
        q = self.query(key)

        print("Status: ", q.Status(),
            "\nMin->",q.Value().Min(),
            "\nMax->",q.Value().Max(),
            "\nMean->",q.Value().Mean(), "\n")


pool_name = "myTabulatorTable"
pool_size = int(1e9)

# create a session and create a pool
def create_pool_and_data():
# -- main line
    tab = Tabulator(ip=sys.argv[1],port=11911)
    tab.create_pool(pool_name, pool_size)
    tab.add_sample("manchester", 1.0)
    tab.add_sample("manchester", 1.0)
    tab.add_sample("manchester", 1.0)
    tab.add_sample("manchester", 5.0)
    tab.add_sample("london", 555.5)

    tab.print_query("manchester")

# open the pool again tab.open_pool()
# create a session and open existing pool - query this pool
def reopen_pool_and_query():
# -- main line
    print ("reopen the connection with MCAS")
    tab = Tabulator(ip=sys.argv[1],port=11911)
    print ("reopen the pool")
    tab.create_pool(pool_name, pool_size)
    print ("Query")
    tab.print_query("manchester")


create_pool_and_data()
reopen_pool_and_query()




