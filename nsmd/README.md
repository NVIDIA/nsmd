# GPM and round robin polling code flow

## GPM / Round Robin cases
Case 1: Device doesn't contain gpm sensors but some round robin sensors(all devices except gpu)  
Case 2: Device doesn't contain round robin sensors but some gpm (currently we dont have such scenario but for future)  
Case 3: Device contain some gpm and some round robin sensor  
Case 4: Device doesn't contain any gpm or round robin(ERoT)  

### Variables
```
alias rr=nsmDevice->roundRobinSensors
alias gpm=nsmDevice->gpmSensors
size_t gpmSensorsSize
size_t roundRobinSensorsSize
bool gpmSensorNeedUpdate
PollingType pollingType
PollingType nextPollingType
shared_ptr<NsmObject> sensor
```

## Case 1
1. Before loop start
```
gpmSensorsSize = 0, 
roundRobinSensorsSize = 5, 
gpmSensorNeedUpdate = true, 
pollingType = RoundRobin, 
nextPollingType = RoundRobin
sensor = nullptr
```
2. First iteration:
```
sensor = rr->front()
roundRobinSensorsSize = 4
nextPollingType = RoundRobin
rr->pop_front()
needsUpdate = i.e. true
calling sensor->update()
rr->push_back()
gpmSensorNeedUpdate = true
```
3. Second iteration:
```
sensor = rr->front()
roundRobinSensorsSize = 3
nextPollingType = RoundRobin
rr->pop_front()
needsUpdate = i.e. false
rr->push_back()
sleep 20ms
continue
gpmSensorNeedUpdate = false
```

## Case 2
1. Before loop start
```
gpmSensorsSize = 5,
roundRobinSensorsSize = 0,
gpmSensorNeedUpdate = true,
pollingType = GpuPerformanceMonitoring,
nextPollingType = GpuPerformanceMonitoring
sensor = nullptr
```
2. First iteration:
```
sensor = gpm->front()
gpmSensorsSize = 4
nextPollingType = GpuPerformanceMonitoring
gpm->pop_front()
gpm->push_back()
needsUpdate = i.e. true
calling sensor->update()
gpmSensorNeedUpdate = true
```
3. Second iteration:
```
sensor = not-null
gpmSensorsSize = 4
nextPollingType = GpuPerformanceMonitoring
gpm->pop_front()
gpm->push_back()
needsUpdate = i.e. false
continue
gpmSensorNeedUpdate = false
```

## Case 3
1. Before loop start
```
gpmSensorsSize = 1,
roundRobinSensorsSize = 2,
gpmSensorNeedUpdate = true,
pollingType = GpuPerformanceMonitoring,
nextPollingType = GpuPerformanceMonitoring
sensor = nullptr
```
2. First iteration (GPM):
```
sensor = gpm->front()
gpmSensorsSize = 0
nextPollingType = RoundRobin
gpm->pop_front()
gpm->push_back()
needsUpdate = i.e. true
calling sensor->update()
gpmSensorNeedUpdate = true
```
3. Second iteration (RoundRobin):
```
sensor = rr->front()
roundRobinSensorsSize = 1
nextPollingType = RoundRobin
rr->pop_front()
needsUpdate = i.e. true
calling sensor->update()
rr->push_back()
gpmSensorNeedUpdate = true
```
4. Third iteration (RoundRobin):
```
sensor = rr->front()
roundRobinSensorsSize = 0
nextPollingType = RoundRobin
rr->pop_front()
needsUpdate = i.e. false
rr->push_back()
gpmSensorNeedUpdate = false
sleep 20ms
continue
```


## Case 4
1. Before loop start
```
gpmSensorsSize = 0,
roundRobinSensorsSize = 0,
gpmSensorNeedUpdate = true,
pollingType = RoundRobin,
nextPollingType = RoundRobin
sensor = nullptr
```
2. First iteration:
```
if (gpmSensorsSize == 0 && roundRobinSensorsSize == 0) {
    break
}
```