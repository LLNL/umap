import os
import subprocess
import sys
#import _thread
import time
from threading import Timer, Thread


class App:
    def __init__(self, name, command, tout, ts=""):
        self.name = name
        self.cmd = command
        self.timeout = tout
        self.state = "Not running"
        self.refs = 0
        self.term_string = ts

    def set_environ(self):
        index = 0
        for str in self.cmd:
            var = str.split("=")
            if len(var) != 2:
                return index
            else:
                os.putenv(var[0], var[1])
                print("Setting environment " + var[0] + "with" + var[1])
                index = index + 1

    def inc_refs(self):
        self.refs = self.refs + 1
        print("refs =",self.refs)

    def run(self):
        cmd_index = self.set_environ()
        run_args = self.cmd[cmd_index].split(" ")
        term_count = 0
        process = subprocess.Popen(
            run_args,
            stderr=subprocess.STDOUT,
            stdout=subprocess.PIPE)
        timer = Timer(self.timeout, process.kill)
        timer.start()
        if process.poll() is None:
            time.sleep(10)
            print("Setting the state to Running")
            self.state = "Running"
        for stdout_line in iter(process.stdout.readline, ""):
            if process.poll() is not None:
                self.state = "Not running"
                break
            else:
                print(stdout_line)
                if(self.term_string!=""):
                    print(self.term_string)
                    if (str(stdout_line).find(self.term_string)!=-1):
                        term_count = term_count + 1
                        print("Termination detected ")
                        if term_count == self.refs:
                            print("Terminating "+self.name)
                            process.terminate()
        timer.cancel()

    def get_state(self):
        print("Getting the state value from object "+self.name)
        return self.state


class Umap_Server(App):
    def __init__(self, name, command, tout):
        super().__init__(name, command, tout,"terminate_handler Done")
        self.state = "Not running"

    def start(self):
        self.run()


class Umap_bfs(App):
    def __init__(self, name, depapp, command, tout):
        self.dep = depapp
        super().__init__(name, command, tout)

    def start(self):
        count = 10
        while self.dep.get_state() == "Not running" and count > 0:
            print("Going to sleep for another round")
            time.sleep(count)
            count = count - 1
        if count != 0:
            self.dep.inc_refs()
            self.run()
        else:
            print("Can't run bfs as the umap-server does not exist")


def get_umap_path():
    umap_path = ""
    if os.getenv('UMAPROOT'):
        umap_path = os.getenv('UMAPROOT')
    else:
        cwd = os.getcwd().split("/")
        for str in cwd:
            umap_path += str + "/"
            if str == "umap":
                break
    return umap_path


def main():
    umap_path = get_umap_path()
    print("Setting UMAP Root = " + umap_path)
    server_cmd = [
        "UMAP_PAGESIZE=524288",
        "OMP_NUM_THREADS=48",
        "UMAP_BUFSIZE=270336",
        "OMP_SCHEDULE=static",
        "UMAP_PAGE_FILLERS=48",
        "UMAP_PAGE_EVICTORS=24",
        umap_path +
        "install/bin/umap-server"]
    serv = Umap_Server("Umap-Server",server_cmd, 5000)
    bfs_cmd = [
        "UMAP_PAGESIZE=524288",
        "OMP_NUM_THREADS=48",
        "UMAP_BUFSIZE=270336",
        "OMP_SCHEDULE=static",
        "UMAP_PAGE_FILLERS=48",
        "UMAP_PAGE_EVICTORS=24",
        "/home/sarkar6/dst-pmemio/umap-apps/install/bin/run_bfs -n 1073741823 -m 34359738368 -g /mnt/ssd/bfs_scale_30/csr_graph_file_30"]
    bfs_cmd_2 = [
        "UMAP_PAGESIZE=524288",
        "OMP_NUM_THREADS=48",
        "UMAP_BUFSIZE=270336",
        "OMP_SCHEDULE=static",
        "UMAP_PAGE_FILLERS=48",
        "UMAP_PAGE_EVICTORS=24",
        "/home/sarkar6/dst-pmemio/umap-apps/install/bin/run_bfs -n 1073741823 -m 34359738368 -o 2 -g /mnt/ssd/bfs_scale_30/csr_graph_file_30"]
    threads = []
    bfs_client = Umap_bfs("BFS",serv, bfs_cmd, 5000)
    bfs_client_2 = Umap_bfs("BFS1",serv, bfs_cmd_2, 5000)
    t_serv = Thread(target=Umap_Server.start, args=(serv,))
    t_serv.start()
    t_client = Thread(target=Umap_bfs.start,args=(bfs_client,))
    t_client.start()
    t_client_2 = Thread(target=Umap_bfs.start,args=(bfs_client_2,))
    t_client_2.start()
    threads.append(t_serv)
    threads.append(t_client)
    threads.append(t_client_2)
    for t in threads:
        t.join()
    
if __name__ == "__main__":
    main()
