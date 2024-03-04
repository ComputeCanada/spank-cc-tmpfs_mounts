Name:           spank-cc-tmpfs_mounts
Version:        1.3
Release:        1%{?dist}
Summary:        SPANK plugin to create in-memory, in-cgroup private tmpfs mounts per each job of users.

Group:          System Environment/Base
License:        MIT
URL:            https://github.com/ComputeCanada/spank-cc-tmpfs_mounts
Source:         spank-cc-tmpfs_mounts-%{version}.tar.gz

%bcond_with selinux

BuildRequires:  slurm-devel
Requires:       slurm-slurmd

%if %{with selinux}
BuildRequires:  libselinux-devel
Requires:       libselinux
%endif

%global _prefix /opt/software/slurm
%define debug_package %{nil}
%description

%prep
%setup -q

%build
CPPFLAGS="-I%{_prefix}/include" make %{?with_selinux:WITH_SELINUX=1}

%install
install -D -m755 cc-tmpfs_mounts.so %{buildroot}/%{_libdir}/slurm/cc-tmpfs_mounts.so

%files
%{_libdir}/slurm/cc-tmpfs_mounts.so

%changelog
