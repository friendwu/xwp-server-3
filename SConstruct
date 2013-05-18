import os
import shutil

def basename(src):
    return src.split('.')[0]

def objname(prefix, src):
    return prefix+'-'+basename(src)

class LocalEnvironment(Environment):
    def Objects(self, src):
        return [self.Object(s) for s in src]

    def Objects(self, name, src):
        return [self.Object(objname(name, s), s) for s in src]

    def SharedObjects(self, src):
        return [self.SharedObject(s) for s in src]

    def SharedObjects(self, name, src):
        return [self.SharedObject(objname(name, s), s) for s in src]

    def Executable(self, name, src):
        return self.Program(name, self.Objects(name, src))

env = LocalEnvironment(CC='gcc', CCFLAGS=['-g', '-Wall'])
Export('env')

SConscript('src/SConscript', variant_dir='build', duplicate=0)

def gen_initrc(target=None, source=None, env=None):
	shutil.copyfile(str(source[0]), str(target[0]))
	os.system('update-rc.d %s defaults 99' % os.path.basename(str(target[0])))

initrc_builder = Builder(action = gen_initrc)
env.Append(BUILDERS = {'BuildInitrc': initrc_builder})

Alias('install', [
					env.Install('/usr/local/xwp/', 'conf/'), 
					env.Install('/usr/local/xwp/', 'static/'), 
				  	env.Install('/usr/local/xwp/sbin/', 'build/main/xwp'),
				  	env.Install('/usr/local/include/', 'zlog.h'),
				  	env.Install('/usr/local/xwp/', 'modules/'),
				  	env.Install('/usr/local/lib/', Glob('libzlog.so*')),
				  	env.BuildInitrc('/etc/init.d/xwp', 'xwp_initrc'),
				])

