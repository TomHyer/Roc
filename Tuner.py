import sys
import os
import random
import shutil
import re
import subprocess
import threading
import time
import bisect
import math

ResultFile = "RocTuning.csv"
RunName = "ParametersDescription"
GroupSizes = [20]
Initial = [24, 24, 24, 0, 32, 20, 8, 0, -32, -16, 0, 0, -4, 18, 40, 0, 28, 32, 36, 0]
Delta = [20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20]
Lambda = 1.0

Current = [24, 24, 24, -10, 32, 20, 8, 10, -32, -16, 0, 10, -4, 18, 40, -10, 28, 32, 36, 10]

sys.path.append("C:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\Common7\\Tools")
TcecFile = 'C:\\Users\\Tom\\Downloads\\TCEC_Season_1_-_Elite-Match.pgn'
ParamsFile = 'TunerParams.inc'
NumPairs = 5
Denom = 2000
DenomInc = 10
LambdaMax = 2.0

def CppForm(pp):
    retval = ''
    (ip, ig) = (0, 0)
    for ng in GroupSizes:
        ig += 1
        retval += 'static const std::array<int, ' + str(ng) + '> TunerParams' + str(ig) + ' = '
        next = '{'
        for ii in range(ng):
            retval += next + ' ' + str(round(pp[ip]))
            ip += 1
            next = ','
        retval += " };\n"
    return retval

def Randomized(pp, qq):
    return [2 * pp[ii] - qq[ii] + Lambda * Delta[ii] * random.normalvariate(0, 1) for ii in range(len(pp))]

def MakeExe(pp, en, debug):
    header = CppForm(pp)
    print("Using parameters:\n " + header)
    header += "/* Current: \n" + CppForm(Current) + "*/\n"
    if os.path.exists(en):
        os.remove(en)
    config = debug and 'Debug' or 'Release'
    builtExe = 'x64/' + config + '/Roc.exe'
    if os.path.exists(builtExe):
        os.remove(builtExe)
    with open(ParamsFile, 'w') as dst:
        dst.writelines(header)

    subprocess.call("C:\\Program Files (x86)\\MSBuild\\14.0\\Bin\\msbuild.exe Roc.vcxproj /p:Configuration=" + config + " /p:Platform=x64 /p:VisualStudioVersion=14.0 /t:Build")
    shutil.copyfile(builtExe, en)

# courtesy of Stack Overflow
class Command(object):
    def __init__(self, cmd):
        self.cmd = cmd
        self.process = None

    def run(self, timeout):
        dst = [None]
        def target():
            print('Thread started')
            self.process = subprocess.Popen(self.cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
            dst[0] = self.process.communicate()[0]
            print('Thread finished')

        thread = threading.Thread(target=target)
        thread.start()

        thread.join(timeout)
        if thread.is_alive():
            print('Unterminated thread; aborting')
            self.process.terminate()
            # thread.join()  # leak the thread, since our process is being shut down anyway

        return dst[0]

def RunMatch(en1, en2, tc):
    matchP = re.compile('pipe reader')  # 'pipe reader was terminated' -- seems to be a windows process error

    cmd = "C:\\Users\\Tom\\Downloads\\cutechess-cli\\cutechess-cli.exe"
    cmd += ' -engine name=c1 cmd=' + en1 + ' proto=uci' 
    cmd += ' -engine name=c2 cmd=' + en2 + ' proto=uci'
    cmd += ' -each tc=' + tc + ' -rounds ' + str(2 * NumPairs)
    cmd += ' -sprt elo0=-20 elo1=20 alpha=0.1 beta=0.1 -resign movecount=3 score=300 -draw movenumber=34 movecount=6 score=15'
    cmd += ' -openings file="' + TcecFile + '" order=random plies=6 -repeat'

    while True:
        print(cmd)
        task = Command(cmd)
        result = task.run(120 * NumPairs)
        result = str(result)
        print(result)
        if (not result) or matchP.search(result):   # internal failure, don't trust the result
            print("Ignoring that")
        else:
            return result


if __name__ == "__main__":
    debug = len(sys.argv) > 1 and sys.argv[1] == 'debug'
    tc = debug and '40+0.1' or '15+0.04'

    done = False
    with open(ResultFile, "a") as results:
        while not done:
            try:
                # create parameters for two contestants
                pp1 = Randomized(Initial, Initial);
                pp2 = Randomized(Current, Initial);
                print('Building c1')
                MakeExe(pp1, "c1.exe", debug)
                print('Building c2')
                MakeExe(pp2, "c2.exe", debug)

                matchS = re.compile('Score of c1 vs c2\: ([0-9]+) - ([0-9]+) - ([0-9]+)')
                matchX = re.compile('connect')  # match 'disconnects' or 'connection stalls' -- an engine failed
                matchF = re.compile('atal error')
                print('Starting matches')
                result = RunMatch("c1.exe", "c2.exe", tc)
                time.sleep(1)
                if matchF.search(result):
                    print("Failed")
                    break   # end of match loop

                crash = matchX.search(result)
                if crash:
                    matchW1 = re.compile('c1 vs c2[^\n]+connect')
                    matchW2 = re.compile('1-0[^\n]+connect')
                    crash2 = (not matchW1.search(result)) == (not matchW2.search(result))
                    print(crash2 and 'C2 crashed' or 'C1 crashed')
                    crasher = crash2 and c2 or c1
                    break   # end of match loop

                score = None
                for score in re.finditer(matchS, result):
                    pass
                # now score holds the last match
                if not score:
                    print("Can't find score -- result text follows")
                    print(result)
                    break   # match loop failed

                print('Results:  ' + score.group(0))
                (w1, w2, d) = [int(score.group(i)) for i in range(1,4)]
                s1 = 2 * w1 + d
                s2 = 2 * w2 + d
                ad1 = (s1 - s2) / 2
                print('c1 wins by ' + str(ad1))
                
                results.write(RunName + "," + str(time.time()) + "," + "|".join(str(round(p)) for p in pp1) + "," + "|".join(str(round(p)) for p in pp2) + "," + ",".join([str(w1),str(w2),str(d)]) + "\n")
                results.flush()

                for ip in range(len(Initial)):
                    Current[ip] += ad1 * (pp1[ip] - pp2[ip]) * NumPairs / Denom
                print('Current parameters: ' + CppForm(Current))
                Denom += DenomInc
                if not ad1:
                    Lambda *= 1.025
                    if LambdaMax and Lambda > LambdaMax:
                        Lambda = LambdaMax
                if abs(ad1) > NumPairs:
                    Lambda *= 0.9
                print('Lambda = ' + str(Lambda))
            except KeyboardInterrupt:
                print('Denom = ' + str(Denom))
                print('Initial = ' + str(Initial))
                print('Current = ' + str(Current))
                print("Really quit?")
                done = input()
                done = (done[:1] == 'y')
