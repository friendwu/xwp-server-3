import os

OUTPUT_BASE = 'obj'

def basename(name):
    return name.split('.')[0]

class target:
    def __init__(self, env, name):
        self.env = env
        self.name = name
        self.outd = OUTPUT_BASE+os.sep+name+os.sep
        self.__prepare()

    def __prepare(self):
        if not os.path.exists(self.outd):
            os.makedirs(self.outd)

    def Object(self, name):
        return self.env.Object(self.outd + basename(name), name)

    def Objects(self, src):
        return [self.Object(s) for s in src]

    def SharedObject(self, name):
        return self.env.SharedObject(self.outd + basename(name), name)

    def SharedObjects(self, src):
        return [self.SharedObject(s) for s in src]

globalEnv = Environment(CC='gcc', CCFLAGS=['-g', '-Wall'])

commonSrc = ['buf.c', 'array.c', 'pool.c', 'http.c', 'conf.c']

## server
serverEnv = globalEnv.Clone(LIBS=['dl', 'pthread'])
module = target(serverEnv, 'xwp')
serverSrc = ['utils.c', 'main.c', 'connection.c', 'server.c']
server = serverEnv.Program(module.name, module.Objects(commonSrc) + serverSrc)

## module_default
libEnv = globalEnv.Clone(CCPATH='.')
module = target(libEnv, 'module_default')
libSrc = ['module_default.c']
lib = libEnv.SharedLibrary(module.name, module.SharedObjects(commonSrc) + libSrc)

## txwp
serverTestEnv = serverEnv.Clone()
module = target(serverTestEnv, 'txwp')
serverTestEnv.Append(CCFLAGS='-DGUARD_DISABLE')
server_test = serverTestEnv.Program(module.name, module.Objects(commonSrc + serverSrc))

## pool_test
poolTestEnv = globalEnv.Clone()
module = 'pool_test'
poolTestEnv.Append(CCFLAGS='-DPOOL_TEST')
pool_test = poolTestEnv.Program(module, poolTestEnv.Object('pool.c'))

globalEnv.Alias('test', [server_test, pool_test])
globalEnv.Default([server, lib])

