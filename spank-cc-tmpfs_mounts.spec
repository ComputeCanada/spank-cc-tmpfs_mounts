Name:           spank-cc-tmpfs_mounts
Version:        1.1
Release:        1%{?dist}
Summary:        SPANK plugin to create in-memory, in-cgroup private tmpfs mounts per each job of users.

Group:          System Environment/Base
License:        MIT
URL:            https://github.com/ComputeCanada/spank-cc-tmpfs_mounts
Source:         spank-cc-tmpfs_mounts-%{version}.tar.gz

BuildRequires:  slurm-devel
Requires:       slurm-slurmd


%global _prefix /opt/software/slurm
%define debug_package %{nil}
%description

%prep
%setup -q

%build
CPPFLAGS="-I%{_prefix}/include" make

%install
install -D -m755 cc-tmpfs_mounts.so %{buildroot}/%{_libdir}/slurm/cc-tmpfs_mounts.so

%files
%{_libdir}/slurm/cc-tmpfs_mounts.so

%changelog
