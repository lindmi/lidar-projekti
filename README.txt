TODO:
    - Rotate thread function
    - Measue thread function

    - Measure full rotation cycle time with X motor speed
    
    algorithm one direction motor;
     - Rotate for full cycle and measure values every X ms.
     - calculate at what time the highest value or value group was measured and rotate for said time to get the general time.

    slow, inaccurate simple

    algorith two direction motor;
     - rotate full cycle and measue values every X ms.
         - Calculate at what time the highest value or value group was measured and rotate for said time + X ms.
         - Calculate what time highest value or value group was measured and rotate to opposite direction for said time + X ms.
         - repeat until time is minimal.

    fast, more accurate, more complex

    - Send data to mqtt broker when the brighest light source is located
    - do something with the data at mqtt
