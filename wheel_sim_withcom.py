import pychrono.core as chrono
import pychrono.irrlicht as irr
import math
import csv
import os

system = chrono.ChSystemSMC()
system.SetGravitationalAcceleration(chrono.ChVector3d(0, -9.81, 0))
system.SetCollisionSystemType(chrono.ChCollisionSystem.Type_BULLET)

mat_ground = chrono.ChContactMaterialSMC()
mat_ground.SetFriction(0.8)       
mat_ground.SetYoungModulus(2e7)   

ground = chrono.ChBodyEasyBox(40.0, 2.0, 40.0, 1000.0, True, True, mat_ground)
ground.SetPos(chrono.ChVector3d(0, -1.0, 0)) 
ground.SetFixed(True) 
system.Add(ground)

ground.GetVisualShape(0).SetTexture(chrono.GetChronoDataFile("textures/checker2.png"))

wheel_radius = 0.125  
target_speed_kmh = 8.0
target_speed_ms = target_speed_kmh / 3.6
target_omega = target_speed_ms / wheel_radius 

total_test_mass = 260.0 

mesh_path = "new_wheel_for_sure.obj"
if not os.path.exists(mesh_path):
    print(f"\n[ERROR] Could not find: '{mesh_path}'")
    exit()

mat_wheel = chrono.ChContactMaterialSMC()
mat_wheel.SetFriction(0.8)
mat_wheel.SetYoungModulus(1e7)

wheel_body = chrono.ChBodyEasyMesh(
    mesh_path, 
    1000.0,  
    True, True, True, mat_wheel      
)

wheel_body.SetMass(5.0) 
wheel_body.SetInertiaXX(chrono.ChVector3d(0.1, 0.1, 0.1))
wheel_body.SetPos(chrono.ChVector3d(0, wheel_radius + 0.05, 0))
system.Add(wheel_body)

carrier = chrono.ChBody()
carrier.SetMass(total_test_mass - 5.0) 
carrier.SetPos(wheel_body.GetPos())
system.Add(carrier)

align_link = chrono.ChLinkLockAlign()
align_link.Initialize(carrier, ground, chrono.ChFramed(carrier.GetPos()))
system.AddLink(align_link)

motor = chrono.ChLinkMotorRotationSpeed()
motor.Initialize(wheel_body, carrier, chrono.ChFramed(wheel_body.GetPos(), chrono.QuatFromAngleY(math.pi/2)))
motor.SetSpeedFunction(chrono.ChFunctionConst(target_omega))
system.AddLink(motor)

vis = irr.ChVisualSystemIrrlicht()
vis.AttachSystem(system)
vis.SetWindowSize(1280, 720)
vis.SetWindowTitle("Team Mushak - Rigid Ground Baseline Test")
vis.Initialize()
vis.AddTypicalLights()
vis.AddSkyBox()
vis.AddCamera(chrono.ChVector3d(2.0, 1.0, -2.5), chrono.ChVector3d(0, 0, 0))

output_csv = "pakka_pakka_sim_results.csv"
print(f"\n[STARTING] Logging full Mushak format data to {output_csv}...")

with open(output_csv, mode='w', newline='') as file:
    writer = csv.writer(file)
    writer.writerow([
        "Time (s)", "Speed_Target (km/h)", "Actual_Speed (km/h)", 
        "Sinkage (m)", "Slip Ratio (%)", "Vertical Load (kN)", 
        "Drawbar Pull (kN)", "Tractive Efficiency (%)", "Status (Compliance)"
    ])

    step_size = 1e-3
    while vis.Run():
        vis.BeginScene()
        vis.Render()
        vis.EndScene()
        
        time = system.GetChTime()
        vel_ms = wheel_body.GetPosDt().x
        vel_kmh = vel_ms * 3.6
        
        motor_torque = abs(motor.GetMotorTorque())
        tractive_effort_n = motor_torque / wheel_radius
        drawbar_pull_kn = tractive_effort_n / 1000.0
        
        sinkage = 0.05 - (wheel_body.GetPos().y - wheel_radius)
        
        if target_speed_ms > 0:
            slip_ratio = (1.0 - (vel_ms / target_speed_ms)) * 100.0
        else:
            slip_ratio = 0.0
            
        input_power = motor_torque * target_omega
        output_power = tractive_effort_n * vel_ms
        if input_power > 0.01:
            tractive_eff = (output_power / input_power) * 100.0
        else:
            tractive_eff = 0.0
            
        is_compliant = (vel_kmh >= 1.0) and (vel_kmh <= 10.0) and (slip_ratio < 80.0) and (slip_ratio > -100.0) and (sinkage > -2.5)
        
        if round(time, 3) % 0.05 == 0:
            writer.writerow([
                round(time, 3), 
                target_speed_kmh, 
                round(vel_kmh, 2), 
                round(sinkage, 4), 
                round(slip_ratio, 2), 
                2.5,  
                round(drawbar_pull_kn, 3), 
                round(tractive_eff, 2), 
                "TRUE" if is_compliant else "FALSE"
            ])

        system.DoStepDynamics(step_size)