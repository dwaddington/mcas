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
        self.pool = self.session.create_pool("myTabulatorTable",int(1e9),100)

    def __del__(self):
        self.pool.close()

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
        q = tab.query(key)

        print("Status: ", q.Status(),
            "\nMin->",q.Value().Min(),
            "\nMax->",q.Value().Max(),
            "\nMean->",q.Value().Mean(), "\n")


                

# -- main line


tab = Tabulator(ip=sys.argv[1],port=11911)

tab.add_sample("manchester", 1.0)
tab.add_sample("manchester", 1.0)
tab.add_sample("manchester", 1.0)
tab.add_sample("manchester", 5.0)
tab.add_sample("london", 666.6)

tab.print_query("manchester")




