from matplotlib import pyplot as plt
import matplotlib.animation as animation
import numpy as np
import serial

from telemetry.parser import *

plot_registry = {}

class BasePlot():
  def __init__(self, indep_def, dep_def, indep_span, subplot, hide_indep_axis=False):
    self.indep_span = indep_span
    self.indep_name = indep_def.internal_name
    self.dep_name = dep_def.internal_name
    if dep_def.limits[0] != dep_def.limits[1]:
      self.limits = dep_def.limits
    else:
      self.limits = None
      
    self.subplot = subplot
    self.subplot.set_title("%s: %s (%s)"           
                           % (dep_def.internal_name, dep_def.display_name, dep_def.units))
    
    plt.setp(subplot.get_xticklabels(), visible=not hide_indep_axis)
    
  def update_from_packet(self, packet):
    raise NotImplementedError 

  def update_show(self, packet):
    raise NotImplementedError
  
  def set_indep_range(self, packet):
    raise NotImplementedError
    
class NumericPlot(BasePlot):
  def __init__(self, indep_def, dep_def, indep_span, subplot, hide_indep_axis=False):
    super(NumericPlot, self).__init__(indep_def, dep_def, indep_span, subplot, hide_indep_axis)
    self.line, = subplot.plot([0])
    
    self.indep_data = deque()
    self.dep_data = deque()
  
  def update_from_packet(self, packet):
    assert isinstance(packet, DataPacket)
    data_names = packet.get_data_names()
    
    if self.indep_name in data_names and self.dep_name in data_names:
      latest_indep = packet.get_data(self.indep_name)
      self.indep_data.append(latest_indep)
      self.dep_data.append(packet.get_data(self.dep_name))
      
      indep_cutoff = latest_indep - self.indep_span
      
      while self.indep_data[0] < indep_cutoff:
        self.indep_data.popleft()
        self.dep_data.popleft()
        
  def update_show(self):
      self.line.set_xdata(self.indep_data)
      self.line.set_ydata(self.dep_data)
      
      if self.limits is not None: 
        minlim = self.limits[0]
        maxlim = self.limits[1] 
      else:
        if not self.dep_data:
          return
        minlim = min(self.dep_data)
        maxlim = max(self.dep_data)
        if minlim < 0 and maxlim < 0:
          maxlim = 0
        elif minlim > 0 and maxlim > 0:
          minlim = 0
      rangelim = maxlim - minlim
      minlim -= rangelim / 20 # TODO make variable padding
      maxlim += rangelim / 20
      self.subplot.set_ylim(minlim, maxlim)

  def set_indep_range(self, indep_range):
    self.subplot.set_xlim(indep_range)

plot_registry[NumericData] = NumericPlot

class WaterfallPlot(BasePlot):
  def __init__(self, indep_def, dep_def, indep_span, subplot, hide_indep_axis=False):
    super(WaterfallPlot, self).__init__(indep_def, dep_def, indep_span, subplot, hide_indep_axis)
    self.count = dep_def.count
    
    self.x_mesh = [0] * (self.count + 1)
    self.x_mesh = np.array([self.x_mesh])
    
    self.y_array = range(0, self.count + 1)
    self.y_array = list(map(lambda x: x - 0.5, self.y_array))
    self.y_mesh = np.array([self.y_array])
    
    self.data_array = None

    plt.setp(subplot.get_xticklabels(), visible=not hide_indep_axis)
    
    self.indep_data = deque()
    self.quad = None
  
  def update_from_packet(self, packet):
    assert isinstance(packet, DataPacket)
    data_names = packet.get_data_names()

    if self.indep_name in data_names and self.dep_name in data_names:
      latest_indep = packet.get_data(self.indep_name)
      self.x_mesh = np.vstack([self.x_mesh, np.array([[latest_indep] * (self.count + 1)])])
      self.y_mesh = np.vstack([self.y_mesh, np.array(self.y_array)])
      if self.data_array is None:
        self.data_array = np.array([packet.get_data(self.dep_name)])
      else:
        self.data_array = np.vstack([self.data_array, packet.get_data(self.dep_name)])
        
      indep_cutoff = latest_indep - self.indep_span
      
      while self.x_mesh[0][0] < indep_cutoff:
        self.x_mesh = np.delete(self.x_mesh, (0), axis=0)
        self.y_mesh = np.delete(self.y_mesh, (0), axis=0)
        self.data_array = np.delete(self.data_array, (0), axis=0)

  def update_show(self):
    if self.quad is not None:
      self.quad.remove()
      del self.quad
    
    if self.limits is not None:
      self.quad = self.subplot.pcolorfast(self.x_mesh, self.y_mesh, self.data_array,
          cmap='gray', vmin=self.limits[0], vmax=self.limits[1],
          interpolation='None')
    else:  
      self.quad = self.subplot.pcolorfast(self.x_mesh, self.y_mesh, self.data_array,
          cmap='gray', interpolation='None')

  def set_indep_range(self, indep_range):
    self.subplot.set_xlim(indep_range)

plot_registry[NumericArray] = WaterfallPlot

if __name__ == "__main__":
  timespan = 10000
  independent_axis_name = 'time'
  
  telemetry = TelemetrySerial(serial.Serial("COM61", baudrate=1000000))
  all_plotdata = []
  fig = plt.figure()
  latest_indep = [0]
  
  def update(data):
    telemetry.process_rx()
    plot_updated = False
    
    while True:
      packet = telemetry.next_rx_packet()
      if not packet:
        break
      
      if isinstance(packet, HeaderPacket):
        all_plotdata.clear()
        fig.clf()
        
        indep_def = None
        data_defs = []
        for _, data_def in reversed(sorted(packet.get_data_defs().items())):
          if data_def.internal_name == independent_axis_name:
            indep_def = data_def 
          else:
            data_defs.append(data_def)

        for plot_idx, data_def in enumerate(data_defs):
          if data_def.internal_name == independent_axis_name:
            continue

          ax = fig.add_subplot(len(data_defs), 1, len(data_defs)-plot_idx)
          plotdata = plot_registry[data_def.__class__](
                         indep_def, data_def, timespan, ax, 
                         hide_indep_axis=(plot_idx != 0))
          all_plotdata.append(plotdata)
          
          print("Found dependent data %s" % data_def.internal_name)

      elif isinstance(packet, DataPacket):
        indep_value = packet.get_data(independent_axis_name)
        if indep_value is not None:
          latest_indep[0] = indep_value
          
        for plotdata in all_plotdata:
          plotdata.update_from_packet(packet)
          
        plot_updated = True
      else:
        raise Exception("Unknown received packet %s" % repr(next_packet))
      
    while True:
      next_byte = telemetry.next_rx_byte()
      if next_byte is None:
        break
      try:
        print(chr(next_byte), end='')
      except UnicodeEncodeError:
        pass

    if plot_updated:
      for plotdata in all_plotdata:    
        plotdata.update_show()  
      for plotdata in all_plotdata:
        plotdata.set_indep_range([latest_indep[0] - timespan, latest_indep[0]])
        
  ani = animation.FuncAnimation(fig, update, interval=50)
  plt.show()
