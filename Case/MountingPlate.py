import cadquery as cq

def MountingPlate():
    w = 37.0
    l = 34.0
    g = 1.7
    h = 1.8+g
    
    result = cq.Workplane("XY" ).box(l, w-1, h).edges("|Z").fillet(0.5)
    
    for y in [-w+2.8, w-2.8]:
        rail = cq.Workplane("XY" ).box(l, w, h)
        result = result.cut(rail.translate((6, y, 0)) )
    
    for y in [-w+5.2, w-5.2]:
        rail = cq.Workplane("XY" ).box(l, w, h)
        result = result.cut(rail.translate((30-1.8, y, 0)) )
    
    lhole = cq.Workplane("XY" ).cylinder(h, 4.0/2)
    xlhole = lhole + cq.Workplane("XY" ).box(12.0, 4.0, h).translate((12/2, 0, 0))
    for y in [-29/2+5.5, 29/2-5.5]:
        x = -l/2+32/2+30/2-4.8
        # log("({}, {}) ({}, {})".format(y, x, y + w/2, x + l/2))
        result = result.cut(xlhole.translate((x, y, 0)))
    
    lhole = cq.Workplane("XY" ).cylinder(1.8, 4.0/2+1.2)
    xlhole = lhole + cq.Workplane("XY" ).box(12.0, 4.0+2*1.2, 1.8).translate((12/2, 0, 0))
    xlhole = xlhole.edges(">Z").chamfer(1.2)
    
    for y in [-29/2+5.5, 29/2-5.5]:
        result = result.cut(xlhole.translate((-l/2+32/2+30/2-4.8, y, -0.4-g/2)))
    
    hhole = cq.Workplane("XY" ).cylinder(h, 2.0/2)
    # log("({}, {}), ({}, {})".format(w/2-5.0, -l/2+3.0, w/2-5.0 + w/2, -l/2+3.0 + l/2))
    result = result.cut(hhole.translate((-l/2+3.0, w/2-5.0, 0)))
    
    latchHole = cq.Workplane("XY" ).box(4, 5.0, h)
    result = result.cut(latchHole.translate((-l/2 + 29.5, 0, 0)))
    
    # undercut
    for y in [-28.4/2-5.0, 28.4/2+5.0]:
        rail = cq.Workplane("XY" ).box(l, 10.0, g)
        result = result.cut(rail.translate((6, y, 1.8/2)) )
    
    return result

if 0:
    show_object(MountingPlate())
    cq.exporters.export(MountingPlate(),'MountingPlate.stl')
    cq.exporters.export(MountingPlate(),'MountingPlate.dxf')
    cq.exporters.export(MountingPlate(),'MountingPlate.step')
