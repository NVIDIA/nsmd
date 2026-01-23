**1. What this MR does / why we need it:**

- 
- 

**2. Make sure that you've checked the boxes below before you submit MR:**
Nvidia Service Validator checklist: 
- [ ] Does this MR change the inventory(NSM/EM) ?
- [ ] Does this MR change the PDI (change in interface/change in property name/change in property data type) ?
- [ ] Does this MR contains changes related to shared memory or TAL ?
- [ ] Does this MR change NSM dbus tree, in terms of association changes ?

If answer to any of the question is yes then

* [ ] I have run the Nvidia Service Validator and diff for the reference platform and there is no error ?
       [https://confluence.nvidia.com/display/SPS/Nvidia+Service+Validator+Run+Steps]

### Static and Dynmaic Value validation
- [ ] Static and Dynamic Value validation for the feature is complete and part of the IT
- [ ] Static and Dynamic Value validation for the feature not part of the IT

### Code Checklist

Logging Checklist: 
- [ ] Does this MR add any logs to journal ?

if yes

* [ ] I am using the log framework for throtlled logs (shouldLog api)
* [ ] I am not using the log framework for throtlled logs and justified the reason for not using it  


Sensor Addition Checklist:
- [ ] This MR adds priority sensor
- [ ] This MR adds round robin sensor
- [ ] This MR adds static sensor

Thanks for your MR, you're awesome! :thumbsup:
