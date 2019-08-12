import bisect
import numpy
import os
import random
import sys

import numpy.random as npr
import gnuplot as Gnuplot

random.seed(10)
numpy.random.seed(10)

SETUP_TIME=50
NTPP_RATIO=0.3

FB_SEED_A = 11321
FB_SEED_B = 51607

def info(fmt, *args):
    print(fmt % args)

class RvCacher(object):
    def __init__(self, gen, steps=20000):
        self._gen = gen
        self._all = self._gen(steps)
        self._index = 0
        self._steps = steps

    def get(self, n):
        if (n > self._steps):
            raise "Cannot request more than the steps size."

        ret = []
        if (n >= len(self._all)):
            self._index = 0
            ret = self._all
            n -= len(self._all)
            self._all = self._gen(self._steps)
            info("Caching %d objects", self._steps)

        ret += self._all[:n]
        self._all = self._all[n:]
        return ret

class RvGenerator(object):
    _generators = {}

    @classmethod
    def get(cls, name, func, *args):
        if name in cls._generators:
            return cls._generators[name]
        cls._generators[name] = RvCacher(lambda x: func(*args, size=x).tolist())
        return cls._generators[name]

class Pdf(object):
    def __init__(self, samples):
        self._samples = samples

    def sample(self, n):
        raise "Need to implement the sample function."

    @property
    def samples(self):
        return self._samples

    def to_exact(self):
        return ExactPdfSampler(self.samples)

    def to_uniform(self, buckets):
        return UniformPdfSampler(buckets, self.samples)

    def _copy(self):
        raise "Need to implement the copy function."

    def copy(self):
        return self._copy()

    def sample_one(self):
        return self.sample(1)[0]

class ExponentialSampler(Pdf):
    def __init__(self, rate):
        self._rate = rate

    def sample(self, n):
        return numpy.random.exponential(1.0/self._rate, n).tolist()


class PowerSampler(Pdf):
    def __init__(self, rate):
        self._rate = rate
        self._name = "power-%f" % (1.0/self._rate)
        self._gen = RvGenerator.get(self._name, numpy.random.power, 1.0/self._rate)

    def sample(self, n):
        #print self._gen.get(n)
        return self._gen.get(n)#numpy.random.power(1.0/self._rate, n).tolist()

class MultiplySampler(Pdf):
    def __init__(self, value, other):
        self._other = other
        self._mult = value

    def sample(self, n):
        return map(lambda x: x * self._mult, self._other.sample(n))

class IntSampler(Pdf):
    def __init__(self, other):
        self._other = other

    def sample(self, n):
        return map(int, self._other.sample(n))

class ExactPdfSampler(Pdf):
    def __init__(self, *args, **kwargs):
        super(ExactPdfSampler, self).__init__(*args, **kwargs)

    def sample(self, n):
        return numpy.random.choice(self.samples, n, replace=True).tolist()

    def _copy(self):
        return ExactPdfSampler(self, list(self.samples))

class UniformPdfSampler(Pdf):
    def __init__(self, buckets, *args, **kwargs):
        super(UniformPdfSampler, self).__init__(*args, **kwargs)
        self._nbuckets = buckets
        self._build_buckets()

    def _bucket_for(self, sample):
        return int((sample - self._min) / self._bucket_size)

    def _range_for(self, bucket):
        return [
            self._min + (bucket * self._bucket_size),
            self._min + ((bucket + 1) * self._bucket_size)]

    def _build_buckets(self):
        self._buckets = {}

        self._min = float(min(self.samples))
        self._max = float(max(self.samples))
        self._bucket_size = ((self._max - self._min) / self._nbuckets)

        self._bids = set()

        for sample in self.samples:
            bid = self._bucket_for(sample)
            if bid not in self._buckets:
                self._buckets[bid] = 0
            self._buckets[bid] += 1
            self._bids.add(bid)

        flen = float(len(self.samples))

        self._bucket_keys = sorted(self._bids)
        self._buckets     = map(lambda bid: self._buckets[bid]/flen, self._bucket_keys)

    def sample(self, n):
        bids = numpy.random.choice(
                xrange(len(self._buckets)), 
                n, replace=True, p=self._buckets).tolist()
        return map(
                lambda bid: random.uniform(
                    *self._range_for(self._bucket_keys[bid])), bids)

    def _copy(self):
        return UniformPdfSampler(self._nbuckets, list(self.samples))

class TrafficDistribution(Pdf):
    def __init__(self, high):
        self._high = high

    def sample(self, n):
        return self._high * numpy.random.power(10, 1).tolist()[0]

    def _copy(self):
        return TrafficDistribution(self._high)


class TrafficSummary(object):
    def __init__(self, gen):
        self._gen = gen
        self._dists = {}

    def pair(self, src, dst, tick):
        #if src not in self._dists:
        #    self._dists[src] = {}

        #if dst not in self._dists[src]:
            #self._dists[src][dst] = self._gen(src, dst, tick)

        return self._gen(src, dst, tick) #self._dists[src][dst]

    @classmethod
    def new(cls, dist_gen, nodes):
        dists = {}

        for src in nodes:
            if src not in dists:
                dists[src] = {}

            for dst in nodes:
                if src == dst:
                    continue
                dists[src][dst] = dist_gen(src, dst)

        return cls(dists)


class User(object):
    def __init__(self, start, end, tors, summary):
        self._tors = tors
        self._summary = summary
        self._start = start
        self._end = end

    def tick(self, start):
        mat = {}
        for src in self._tors:
            for dst in self._tors:
                if (src != dst):
                    mat[(src, dst)] = self._summary.pair(src, dst, start)
        return mat


    @property
    def start(self):
        return self._start

    @property
    def end(self):
        return self._end

    @property
    def duration(self):
        return self.end - self.start

class ClampSampler(Pdf):
    def __init__(self, low, high, other):
        self._low = low
        self._high = high
        self._other = other

        def clamp(val):
            if (val > self._high):
                return self._high
            elif (val < self._low):
                return self._low
            return val
        self._clamp = clamp

    def sample(self, n):
        return map(self._clamp, self._other.sample(n))

class ToRSamplerUniform(Pdf):
    def __init__(self, num_tors_per_pod, num_pods, size_dist):
        self._ntpp = num_tors_per_pod
        self._npod = num_pods
        self._size_dist = size_dist

    # Sample uniformly from the pods and the ToRs
    def sample(self, n):
        nt = self._ntpp * self._npod;
        ret = []

        for i in xrange(n):
            ntors = self._size_dist.sample_one()
            ret.append(numpy.random.choice(xrange(0, nt), ntors, replace=False).tolist())

        return ret
    

class ToRSampler(Pdf):
    def __init__(self, num_tors_per_pod, num_pods, size_dist):
        self._ntpp = num_tors_per_pod
        self._npod = num_pods
        self._size_dist = size_dist

    # Sample uniformly from the pods and the ToRs
    def sample(self, n):
        ret = []
        for i in xrange(n):
            tors      = []
            ntors     = self._size_dist.sample_one()
            npods     = ntors / self._ntpp
            last_tors = ntors - npods * self._ntpp

            choices = numpy.random.choice(xrange(self._npod), npods + 1).tolist()
            last_pod = choices[0]
            del choices[0]

            tors += map(lambda x: 
                        map(lambda y: x * self._ntpp + y, 
                            xrange(self._ntpp)),
                        choices)

            last_tor_ids = numpy.random.choice(xrange(0, self._ntpp), last_tors).tolist()
            tors += map(lambda y: last_pod * self._ntpp + y, last_tor_ids),
            ret.append([item for pod in tors for item in pod])
        return ret
    

class TraceBuilder(object):
    def __init__(self, 
            traffic_summary_dist, 
            user_arrival_dist, 
            user_duration_dist,
            user_size_dist,
            rate_limiter = None):
        self._tsd = traffic_summary_dist
        self._uad = user_arrival_dist
        self._udd = user_duration_dist
        self._usd = user_size_dist
        self._duration = 0
        self._rm = rate_limiter

        if rate_limiter == None:
            self._rm = lambda pair, vol: vol


    def _sample_user_arrival_and_duration(self, uat, ticks, interval):
        duration = self._duration
        while (duration < ticks * interval):
            sample = self._uad.sample_one()
            duration += sample
            tors = self._usd.sample_one()
            uat.append(
                User(
                    start=duration,
                    end=duration + self._udd.sample_one(),
                    tors=self._usd.sample_one(),
                    summary=self._tsd.sample_one(),
                    ))

        self._duration = duration       
        return uat

    def build(self, ticks, interval):
        #users = self._sample_user_arrival_and_duration(ticks, interval)

        users = []
        active_users = []
        active_users_end = []
        def _add_next_users(an_idx, users, tick, interval):
            batch = []
            end = (tick + 1) * interval
            while (users[an_idx].start < end):
                index = bisect.bisect_left(active_users_end, users[an_idx].end)
                active_users_end.insert(index, users[an_idx].end)
                active_users.insert(index, users[an_idx])
                an_idx += 1
            return an_idx

        def _del_dead_users(tick, interval):
            end = (tick + 1) * interval
            while (active_users and active_users[0].end < end):
                # print "Deleting :", active_users[0].start, ", ", active_users[0].end, end
                # print "Deleting user: %s" % active_users[0].end
                del active_users[0]
                del active_users_end[0]

        def _clean_up_users(users, tick, index):
            to_remove = []
            subtract_index = 0
            for idx, user in enumerate(users):
                if user.end < tick:
                    to_remove.append(idx)

                    # if we are removing from before the marker
                    # update the index
                    if idx < index:
                        subtract_index += 1

                if user.start > tick:
                    break

            print "Removed: %d users adjusted the index by: %d" % (len(to_remove), subtract_index)

            for i in to_remove[::-1]:
                del users[i]
            return index - subtract_index

        info("Running warm up runs to initiate user traffic.")
        self._duration = -SETUP_TIME
        __idx = 0
        for tick in xrange(-SETUP_TIME, 0):
            self._sample_user_arrival_and_duration(users, tick + 4, interval)
            __idx = _add_next_users(__idx, users, tick, interval)
            _del_dead_users(tick, interval)
            __idx = _clean_up_users(users, tick+1, __idx)
            # We don't actually need to generate traffic for mock runs
            # for index, user in enumerate(active_users):
            #     assert(user.start < tick + 1)
            #     assert(user.end > tick)
            #     for pair, vol in user.tick(1).iteritems():
            #         if pair not in tm:
            #             tm[pair] = 0
            #         tm[pair] += vol[0]


        info("Running traffic runs.")
        for tick in xrange(ticks):
            tm = {}
            self._sample_user_arrival_and_duration(users, tick + 4, interval)
            __idx = _add_next_users(__idx, users, tick, interval)
            for user in active_users:
                #assert user.start < tick + 1
                #assert user.end > tick

                bws = user.tick(tick)
                # print len(bws)
                for pair, vol in bws.iteritems():
                    if pair not in tm:
                        tm[pair] = 0
                    tm[pair] += vol

            _del_dead_users(tick, interval)
            __idx = _clean_up_users(users, tick+1, __idx)

            for pair, vol in tm.iteritems():
                tm[pair] = self._rm(pair, vol)

            yield active_users, tm

def save_tm(tm, num_tors, fname):
    traffic=[]
    with open(fname, "w+") as trace:
        for src in xrange(num_tors):
            for dst in xrange(num_tors):
                if src == dst: continue

                key = (src,dst)
                if (key not in tm):
                    traffic.append("0")
                else:
                    traffic.append(str(tm[key]))

        trace.write("\n".join(traffic))

def stats_gnuplot(tm, num_pods, num_tors_per_pod):
    num_tors = num_pods * num_tors_per_pod
    idx = 0
    pod_tr = {}

    sid = 0
    for spod in xrange(num_pods):
        for stor in xrange(num_tors_per_pod):
            sid += 1
            did = 0
            for dpod in xrange(num_pods):
                for dtor in xrange(num_tors_per_pod):
                    did += 1
                    key = (sid, did)
                    if key not in tm:
                        continue

                    bw = tm[key]
                    idx += 1


                    key = (spod, dpod)
                    if spod == dpod:
                        key = (0, 0)
                    if key not in pod_tr:
                        pod_tr[key] = []
                    pod_tr[key].append(bw)

    data = sorted(pod_tr[(0, 0)])
    dlen = len(data)
    data_max = data[-1]
    y = map(lambda x: (float(x)/dlen), xrange(dlen))
    x = data

    g = Gnuplot.Gnuplot(debug=0);
    g('set terminal png')
    g('set output "test.png"')
    for i in range(1, 9):
        dm = data_max * i / 8
        print dm
        g("set arrow from %d,0 to %d,1 nohead lc rgb 'red'" % (dm, dm))
    g.plot(Gnuplot.Data(zip(x, y)))
    exit(1)

def stats_tm(tm, num_pods, num_tors_per_pod):
    num_tors = num_pods * num_tors_per_pod
    idx = 0
    pod_tr = {}
    pod_tr_count = {}

    sid = 0
    for spod in xrange(num_pods):
        for stor in xrange(num_tors_per_pod):
            sid += 1
            did = 0
            for dpod in xrange(num_pods):
                for dtor in xrange(num_tors_per_pod):
                    did += 1
                    key = (sid, did)
                    if key not in tm:
                        continue

                    bw = tm[key]
                    idx += 1


                    key = (spod, dpod)
                    if key not in pod_tr:
                        pod_tr[key] = 0
                    pod_tr[key] += bw

                    if key not in pod_tr_count:
                        pod_tr_count[key] = 0
                    if bw != 0:
                        pod_tr_count[key] += 1

    intra_pod = {}
    intra_pod_count = {}
    extra_pod = {}
    extra_pod_count = {}

    for pod in xrange(num_pods):
        if pod not in intra_pod:
            intra_pod[pod] = 0
            intra_pod_count[pod] = 0
        intra_pod[pod] += pod_tr[(pod, pod)]
        intra_pod_count[pod] += pod_tr_count[(pod, pod)]

    for spod in xrange(num_pods):
        for dpod in xrange(num_pods):
            if spod == dpod:
                continue
            key = (spod, dpod)
            if spod not in extra_pod:
                extra_pod[spod] = 0
                extra_pod_count[spod] = 0
            if key in pod_tr:
                extra_pod[spod] += pod_tr[key]
                extra_pod_count[spod] += pod_tr_count[key]

    print "IntraPod", intra_pod
    print "ExtraPod", extra_pod

    print "IntraPodCount", intra_pod_count
    print "ExtraPodCount", extra_pod_count


def save_key(num_pods, num_tors_per_pod, fname):
    num_tors = num_pods * num_tors_per_pod

    output = []
    with open(fname, "w+") as keys:
        for src in xrange(num_tors):
            spod = src / num_tors_per_pod
            for dst in xrange(num_tors):
                if (src == dst): continue
                dpod = dst / num_tors_per_pod

                output.append("t%d\tt%s\tp%d\tp%d\t0\t0" % (src, dst, spod, dpod))
        keys.write("\n".join(output))

def usage():
    print """Usage:
    > %s [dir] [# pods] [# tors per pod] [%% user size < 1] [# trace length] [# webserver trace path] [# hadoop trace path]

    default args could be:
    %s . 10 10 0.9 200
    
    """ % (sys.argv[0], sys.argv[0])
    exit(1)

def load_trace(path, length):
    pod_count = 0
    def load_key():
        pod_tor  = {}
        tors = set()
        def add_tor_to_pod(tor, pod):
            pod = int(pod[1:]) # Remove p from the name
            tor = int(tor[1:]) # Remove t from the name

            if pod not in pod_tor:
                pod_tor[pod] = set()
            pod_tor[pod].add(tor)

            return tor, pod

        relation = {}
        with open(os.path.join(path, "key.tsv")) as f:
            index = 0
            for num, line in enumerate(f):
                src, dst, spod, dpod, _, _ = line.split("\t")
                src, spod = add_tor_to_pod(src, spod)
                dst, dpod = add_tor_to_pod(dst, dpod)

                if src == dst:
                    continue

                relation[index] = (src, dst)
                index += 1

        return {k: sorted(list(v)) for k, v in pod_tor.iteritems()}, relation

    def load_traffic(fname, relation, tick, traffic):
        traffic[tick] = {}
        T = traffic[tick]
        with open(os.path.join(path, fname)) as f:
            for num, line in enumerate(f):
                src, dst = relation[num]
                if src not in T:
                    T[src] = {}

                T[src][dst] = float(line)


    pod_tor, traffic_relation = load_key()
    traffic = {}

    traffic_files = [f for f in os.listdir(path) if os.path.isfile(os.path.join(path, f)) and f.find("000") != -1]
    count = len(traffic_files) / length

    for tick, tfile in enumerate(sorted(traffic_files)[::count]):
        info("Loading %s", tfile);
        load_traffic(tfile, traffic_relation, tick, traffic)

    return {
        'pod_count': len(pod_tor),
        'tor_count_for': lambda x: len(pod_tor[x]),
        'located_tor': lambda p, t: pod_tor[p][t],
        'pod_tor': pod_tor,
        'traffic': traffic
    }

class Config(object):
    def __init__(self, args):
        self.DIR = args[0]
        self.NUM_PODS = int(args[1])
        self.NUM_TORS_PER_POD = int(args[2])
        self.MAX_USER_SIZE = int(self.NUM_PODS * self.NUM_TORS_PER_POD * float(args[3]))
        self.TRACE_LENGTH = int(args[4])
        self.WEBSERVER_TRACE = args[5]
        self.HADOOP_TRACE = args[6]
        self.WEBSERVER_TRACE_WEIGHT = 0.5
        self.HADOOP_TRACE_WEIGHT = 0.5

        if (len(args) > 7):
            self.WEBSERVER_TRACE_WEIGHT = float(args[7])
            self.HADOOP_TRACE_WEIGHT = float(args[8])

        # Possibly don't want to change these
        self.MIN_USER_SIZE = self.MAX_USER_SIZE / 5
        self.MAX_USER_DURATION = 100
        self.MAX_USER_ARRIVAL_RATE = 0.1
        self.TRAFFIC_VOLUME_POWER=10

        self._hadoop_trace = None
        self._webserver_trace = None

    @property
    def dir(self):
        return self.DIR

    @property
    def num_pods(self):
        return self.NUM_PODS

    @property
    def num_tors_per_pod(self):
        return self.NUM_TORS_PER_POD

    @property
    def num_tors(self):
        return self.num_pods * self.num_tors_per_pod

    @property
    def max_user_duration(self):
        return self.MAX_USER_DURATION

    @property
    def max_user_arrival_rate(self):
        return self.MAX_USER_ARRIVAL_RATE

    @property
    def traffic_volumn_power_dist_param(self):
        return self.TRAFFIC_VOLUME_POWER

    @property
    def min_user_size(self):
        return self.MIN_USER_SIZE

    @property
    def max_user_size(self):
        return self.MAX_USER_SIZE

    @property
    def trace_length(self):
        return self.TRACE_LENGTH

    @property
    def hadoop_trace_path(self):
        return self.HADOOP_TRACE

    @property
    def hadoop_trace(self):
        print self.hadoop_trace_path, self.trace_length
        if not self._hadoop_trace:
            self._hadoop_trace = load_trace(self.hadoop_trace_path, self.trace_length)
        return self._hadoop_trace

    @property
    def hadoop_trace_prob(self):
        return self.HADOOP_TRACE_WEIGHT

    @property
    def webserver_trace_path(self):
        return self.WEBSERVER_TRACE

    @property
    def webserver_trace(self):
        if not self._webserver_trace:
            self._webserver_trace = load_trace(self.webserver_trace_path, self.trace_length)
        return self._webserver_trace

    @property
    def webserver_trace_prob(self):
        return self.WEBSERVER_TRACE_WEIGHT

def traffic_summary_tunable(intra_to_inter_pod_flow_count, 
                            intra_to_inter_pod_flow_volume,
                            traffic_low, traffic_high,
                            config):
    ntpp = config.num_tors_per_pod
    num_tors = config.num_tors
    ntt = ntpp * (ntpp - 1)
    npp = (num_tors - ntpp) * (num_tors - ntpp)

    prob_flow_pp = float(ntt) / float(npp * intra_to_inter_pod_flow_count)
    prob_flow_tt = 1.0

    if prob_flow_pp > 1.0:
        prob_flow_tt = 1.0/prob_flow_pp
        prob_flow_pp = 1.0

    tvol_high_pp = int(traffic_high / intra_to_inter_pod_flow_volume)
    tvol_low_pp  = int(traffic_low  / intra_to_inter_pod_flow_volume)
    tvol_high_tt = traffic_high
    tvol_low_tt  = traffic_low

    #variables = {'pp': 0, 'pv': 0, 'tt': 0, 'tv': 0, 'ts': set([]), 'ps': set([])}

    def gen(src, dst, tick):
        spod = src/ntpp
        dpod = dst/ntpp

        toss = random.random()
        if (spod == dpod):
            #variables['tt'] += 1
            # intra-pod communication
            if (toss <= prob_flow_tt):
                #variables['tv'] += 1
                #variables['ts'].add((src, dst, ))
                return random.randint(tvol_low_tt, tvol_high_tt)
            return 0
        else:
            #variables['pp'] += 1
            if (toss <= prob_flow_pp):
                #variables['pv'] += 1
                #variables['ps'].add((src, dst, ))
                return random.randint(tvol_low_pp, tvol_high_pp)
            return 0

    return TrafficSummary(gen)

def traffic_summary_default(traffic_low_mult, traffic_high_mult, config):
    def get_pod(tor):
        return tor/config.num_tors_per_pod

    def p2p(pod, nfb_pods):
        return ((pod * FB_SEED_A) + FB_SEED_B) % nfb_pods

    def t2t(tor, nfb_tors):
        return ((tor * FB_SEED_A) + FB_SEED_B) % nfb_tors

    def fb_gen(traffic_dist):
        dist = traffic_dist

        tr   = dist['traffic']
        pc   = dist['pod_count']
        tcf  = dist['tor_count_for']
        lt   = dist['located_tor']

        def gen(src, dst, start_tick):
            spod = get_pod(src)
            dpod = get_pod(dst)

            fspod = p2p(spod, pc)
            fdpod = p2p(dpod, pc)

            fsrc = t2t(src, tcf(fspod))
            fdst = t2t(dst, tcf(fdpod))

            flsrc = lt(fspod, fsrc)
            fldst = lt(fdpod, fdst)

            # have to do one final translation from fsrc and fspod to located tor

            # it could be the case that fsrc and fdst are the same after the translation
            # in that case we just return 0
            if (flsrc == fldst):
                return 0

            return tr[start_tick][flsrc][fldst]
        return gen

    def power_gen(src, dst, start_tick):
        return IntSampler(
                MultiplySampler(random.randint(traffic_low_mult, traffic_high_mult), 
                    PowerSampler(config.traffic_volumn_power_dist_param)))

    hadoop_gen    = fb_gen(config.hadoop_trace)
    webserver_gen = fb_gen(config.webserver_trace)

    #generator = random.choose([hadoop_gen, webserver_gen])
    generator = npr.choice(
            [hadoop_gen, webserver_gen], 1, 
            p=[config.hadoop_trace_prob, config.webserver_trace_prob]).tolist()[0]

    return TrafficSummary(generator)


def rate_limit_by(x):
    def f(pair, vol):
        if vol > x:
            return x
        return vol
    return f

def rate_limit_gentle_by(x):
    def f(pair, vol):
        if vol > x:
            return numpy.random.normal(x, x/20, 1).tolist()[0]
        return vol
    return f

def no_rate_limit():
    def f(pair, vol):
        return vol
    return f

# Default traffic generation parameters
# Setting: try to put user VMs in the same pods
def traffic_scenario_default(config):
    traffic_summaries = ExactPdfSampler([
        traffic_summary_default(1, random.randint(1, 1), config)
        for _ in xrange(100)])

    poisson_arrival  = MultiplySampler(config.max_user_arrival_rate, ExponentialSampler(10))
    poisson_duration = MultiplySampler(config.max_user_duration, PowerSampler(10))
    tor_sampler = ToRSampler(config.num_tors_per_pod, config.num_pods, 
            ClampSampler(config.min_user_size, config.max_user_size, 
                IntSampler(
                    MultiplySampler(config.max_user_size,
                        PowerSampler(10)))))

    rate_limiter = rate_limit_gentle_by(400000);

    return traffic_summaries, poisson_arrival, poisson_duration, tor_sampler, rate_limiter

def traffic_scenario_bad_scheduler(config):
    traffic_summaries = ExactPdfSampler([
        traffic_summary_default(1, random.randint(1, 1), config)
        for _ in xrange(100)])

    poisson_arrival  = MultiplySampler(config.max_user_arrival_rate, ExponentialSampler(10))
    poisson_duration = MultiplySampler(config.max_user_duration, PowerSampler(10))
    tor_sampler = ToRSamplerUniform(config.num_tors_per_pod, config.num_pods, 
            ClampSampler(config.min_user_size, config.max_user_size, 
                IntSampler(
                    MultiplySampler(config.max_user_size,
                        PowerSampler(10)))))

    return traffic_summaries, poisson_arrival, poisson_duration, tor_sampler, None

def traffic_scenario_tunable(config):
    ntpp = config.num_tors_per_pod
    traffic_summaries = ExactPdfSampler([
        traffic_summary_tunable(0.01, 400, 1, random.randint(1, 10000), config)
        for _ in xrange(100)])

    poisson_arrival  = MultiplySampler(config.max_user_arrival_rate, ExponentialSampler(10))
    poisson_duration = MultiplySampler(config.max_user_duration, PowerSampler(10))
    tor_sampler = ToRSamplerUniform(config.num_tors_per_pod, config.num_pods, 
            ClampSampler(config.min_user_size, config.max_user_size, 
                IntSampler(
                    MultiplySampler(config.max_user_size,
                        PowerSampler(0.1)))))

    return traffic_summaries, poisson_arrival, poisson_duration, tor_sampler, None

def traffic_scenario_tunable_fixed(config):
    ntpp = config.num_tors_per_pod
    traffic_summaries = ExactPdfSampler([traffic_summary_tunable(0.13*0.1439, 400, 10000, 10000, config)])
    max_user_size = config.num_tors# - 1

    poisson_arrival  = ClampSampler(0.1, 0.1,
            MultiplySampler(config.max_user_arrival_rate, ExponentialSampler(10)))
    poisson_duration = ClampSampler(4, 4,
            MultiplySampler(config.max_user_duration, PowerSampler(10)))
    tor_sampler = ToRSamplerUniform(config.num_tors_per_pod, config.num_pods, 
            ClampSampler(max_user_size, max_user_size, IntSampler(
                MultiplySampler(config.max_user_size,
                    PowerSampler(0.1)))))

    return traffic_summaries, poisson_arrival, poisson_duration, tor_sampler, None

def main():
    config = Config(sys.argv[1:])
    if not os.path.exists(config.dir):
        os.makedirs(config.dir)

    iterator = TraceBuilder(
                    *traffic_scenario_bad_scheduler(config)
               ).build(ticks=config.trace_length, interval=1)

    for tick, data in enumerate(iterator):
        _, tm = data
        info("Saving %d - %d", tick, tick+1)
        save_tm(tm, config.num_tors, os.path.join(config.dir, ("%08d" % tick) + ".tsv"))
        stats_tm(tm, config.num_pods, config.num_tors_per_pod)
        #if tick > 60:
        #    stats_gnuplot(tm, config.num_pods, config.num_tors_per_pod)
    save_key(config.num_pods, config.num_tors_per_pod, os.path.join(config.dir, "key.tsv"))

if __name__ == '__main__':
    if len(sys.argv) < 8:
        usage()
    main()

# Inadmissible Traffic fix:
#   ... Rate limit the users causing the inadmissibility.
#       Rate limit the ToRs---
#       Rate limit the whole TM
#         - Throw away the TMs
#   ... 

