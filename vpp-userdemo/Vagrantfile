# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure(2) do |config|

  # Pick the right distro and bootstrap, default is ubuntu1404
  config.vm.box = "puppetlabs/ubuntu-14.04-64-nocm"
  vmcpu=(ENV['VPP_VAGRANT_VMCPU'] || 2)
  vmram=(ENV['VPP_VAGRANT_VMRAM'] || 4096)

  # Define some physical ports for your VMs to be used by DPDK
  config.vm.network "private_network", type: "dhcp"

  config.vm.provision :shell, :path => File.join(File.dirname(__FILE__),"install.sh")
  config.vm.provision :shell, :path => File.join(File.dirname(__FILE__),"clearinterfaces.sh")


  # vagrant-cachier caches apt/yum etc to speed subsequent
  # vagrant up
  # to enable, run
  # vagrant plugin install vagrant-cachier
  #
  if Vagrant.has_plugin?("vagrant-cachier")
    config.cache.scope = :box
  end

  # use http proxy if avaiable
  if ENV['http_proxy'] && Vagrant.has_plugin?("vagrant-proxyconf")
   config.proxy.http     = ENV['http_proxy']
   config.proxy.https    = ENV['https_proxy']
   config.proxy.no_proxy = "localhost,127.0.0.1"
  end

  config.vm.provider "virtualbox" do |vb|
      vb.customize ["modifyvm", :id, "--ioapic", "on"]
      vb.memory = "#{vmram}"
      vb.cpus = "#{vmcpu}"
  end
  config.vm.provider "vmware_fusion" do |fusion,override|
    fusion.vmx["memsize"] = "#{vmram}"
    fusion.vmx["numvcpus"] = "#{vmcpu}"
  end
  config.vm.provider "libvirt" do |lv|
    lv.memory = "#{vmram}"
    lv.cpus = "#{vmcpu}"
  end
  config.vm.provider "vmware_workstation" do |vws,override|
    vws.vmx["memsize"] = "#{vmram}"
    vws.vmx["numvcpus"] = "#{vmcpu}"
  end
end
