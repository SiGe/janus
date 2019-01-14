import bisect
import numpy
import os
import random
import sys

random.seed(10)
numpy.random.seed(10)

SETUP_TIME=50

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

    def pair(self, src, dst):
        if src not in self._dists:
            self._dists[src] = {}

        if dst not in self._dists[src]:
            self._dists[src][dst] = self._gen(src, dst)

        return self._dists[src][dst]

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

    def tick(self, ticks):
        mat = {}
        for _ in xrange(ticks):
            for src in self._tors:
                for dst in self._tors:
                    if (src != dst):
                        mat[(src, dst)] = self._summary.pair(src, dst).sample(ticks)
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
            user_size_dist):
        self._tsd = traffic_summary_dist
        self._uad = user_arrival_dist
        self._udd = user_duration_dist
        self._usd = user_size_dist
        self._duration = 0

    def _sample_user_arrival_and_duration(self, uat, ticks, interval):
        duration = self._duration
        while (duration < ticks * interval):
            sample = self._uad.sample_one()
            duration += sample
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
                assert user.start < tick + 1
                assert user.end > tick
                for pair, vol in user.tick(1).iteritems():
                    if pair not in tm:
                        tm[pair] = 0
                    tm[pair] += vol[0]

            _del_dead_users(tick, interval)
            __idx = _clean_up_users(users, tick+1, __idx)

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
    > %s [dir] [# pods] [# tors per pod] [%% user size < 1] [# trace length]

    default args could be:
    %s . 10 10 0.9 200
    
    """ % (sys.argv[0], sys.argv[0])
    exit(1)

def main():
    # NUM_TORS_PER_POD = 10
    # NUM_PODS = 10
    # MAX_USER_SIZE=100
    # MIN_USER_SIZE=1
    # MAX_USER_DURATION=100
    # MAX_USER_ARRIVAL_RATE=0.2
    # TRACE_LENGTH=200
    # TRAFFIC_VOLUME_POWER=10

    DIR = sys.argv[1]
    NUM_PODS = int(sys.argv[2])
    NUM_TORS_PER_POD = int(sys.argv[3])
    MAX_USER_SIZE = int(NUM_PODS * NUM_TORS_PER_POD * float(sys.argv[4]))
    TRACE_LENGTH = int(sys.argv[5])

    # Possibly don't want to change these
    MIN_USER_SIZE = 1
    MAX_USER_DURATION = 100
    MAX_USER_ARRIVAL_RATE = 0.1
    TRAFFIC_VOLUME_POWER=10

    if not os.path.exists(DIR):
            os.makedirs(DIR)

    def traffic_summary(L, H):
        def dist_gen(src, dst):
            return IntSampler(
                    MultiplySampler(random.randint(L, H), 
                        PowerSampler(TRAFFIC_VOLUME_POWER)))
        return TrafficSummary(dist_gen)

    traffic_summaries = ExactPdfSampler([
        traffic_summary(1, random.randint(1, 10000))
        for _ in xrange(100)])

    poisson_arrival  = MultiplySampler(MAX_USER_ARRIVAL_RATE, ExponentialSampler(10))
    poisson_duration = MultiplySampler(MAX_USER_DURATION, PowerSampler(10))
    tor_sampler = ToRSampler(NUM_TORS_PER_POD, NUM_PODS, 
            ClampSampler(MIN_USER_SIZE, MAX_USER_SIZE, 
                IntSampler(
                    MultiplySampler(MAX_USER_SIZE, 
                        PowerSampler(10)))))

    iterator = TraceBuilder(
            traffic_summaries,
            poisson_arrival, 
            poisson_duration, 
            tor_sampler).build(
                ticks=TRACE_LENGTH, interval=1)

    for tick, data in enumerate(iterator):
        pods = {}
        tot_traffic = 0
        users, tm = data
        info("Saving %d - %d", tick, tick+1)
        save_tm(tm, NUM_TORS_PER_POD * NUM_PODS, os.path.join(DIR, ("%08d" % tick) + ".tsv"))
    save_key(NUM_PODS, NUM_TORS_PER_POD, os.path.join(DIR, "key.tsv"))

if __name__ == '__main__':
    if len(sys.argv) != 6:
        usage()

    main()
    #NumpyCacher(numpy.random.choice

