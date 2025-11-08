import { Link, useLocation } from 'react-router-dom'
import { 
  LayoutDashboard, 
  Radio, 
  Sliders, 
  FileUp, 
  GitBranch, 
  Activity, 
  Rss, 
  TrendingUp, 
  Triangle, 
  Zap, 
  GitMerge,
  Gauge,
  Settings 
} from 'lucide-react'
import { cn } from '@/lib/utils'

const navigation = [
  { name: 'Dashboard', href: '/', icon: LayoutDashboard },
  { 
    name: 'Modules', 
    items: [
      { name: 'SV Publishers', href: '/streams', icon: Radio },
      { name: 'Manual Injection', href: '/manual', icon: Sliders },
      { name: 'COMTRADE/CSV', href: '/comtrade', icon: FileUp },
      { name: 'Sequencer', href: '/sequencer', icon: GitBranch },
      { name: 'Analyzer', href: '/analyzer', icon: Activity },
      { name: 'GOOSE Config', href: '/goose', icon: Rss },
      { name: 'Impedance', href: '/impedance', icon: Gauge },
    ]
  },
  {
    name: 'Tests',
    items: [
      { name: 'Ramping', href: '/ramping', icon: TrendingUp },
      { name: 'Distance 21', href: '/distance', icon: Triangle },
      { name: 'Overcurrent 50/51', href: '/overcurrent', icon: Zap },
      { name: 'Differential 87', href: '/differential', icon: GitMerge },
    ]
  },
  { name: 'Settings', href: '/settings', icon: Settings },
]

interface SidebarProps {
  className?: string
}

export function Sidebar({ className }: SidebarProps) {
  const location = useLocation()
  
  return (
    <div className={cn('pb-12 min-h-screen border-r border-gray-200 dark:border-gray-800 bg-white dark:bg-gray-900', className)}>
      <div className="space-y-4 py-4">
        <div className="px-3 py-2">
          <div className="mb-4 px-4">
            <h2 className="text-lg font-semibold tracking-tight">
              Virtual TestSet
            </h2>
            <p className="text-xs text-gray-600 dark:text-gray-400">
              IEC 61850 Test Platform
            </p>
          </div>
          <div className="space-y-1">
            {navigation.map((item) => (
              item.items ? (
                <div key={item.name} className="space-y-1">
                  <h3 className="px-4 py-2 text-sm font-semibold text-gray-600 dark:text-gray-400">
                    {item.name}
                  </h3>
                  {item.items.map((subItem) => {
                    const isActive = location.pathname === subItem.href
                    return (
                      <Link
                        key={subItem.href}
                        to={subItem.href}
                        className={cn(
                          'group flex items-center rounded-md px-3 py-2 text-sm font-medium hover:bg-gray-100 dark:hover:bg-gray-800',
                          isActive 
                            ? 'bg-gray-100 dark:bg-gray-800 text-gray-900 dark:text-gray-100' 
                            : 'text-gray-600 dark:text-gray-400'
                        )}
                      >
                        <subItem.icon className="mr-2 h-4 w-4" />
                        <span>{subItem.name}</span>
                      </Link>
                    )
                  })}
                </div>
              ) : (
                <Link
                  key={item.href}
                  to={item.href}
                  className={cn(
                    'group flex items-center rounded-md px-3 py-2 text-sm font-medium hover:bg-gray-100 dark:hover:bg-gray-800',
                    location.pathname === item.href 
                      ? 'bg-gray-100 dark:bg-gray-800 text-gray-900 dark:text-gray-100' 
                      : 'text-gray-600 dark:text-gray-400'
                  )}
                >
                  <item.icon className="mr-2 h-4 w-4" />
                  <span>{item.name}</span>
                </Link>
              )
            ))}
          </div>
        </div>
      </div>
    </div>
  )
}
