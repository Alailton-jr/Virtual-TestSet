import { useState } from 'react'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Badge } from '@/components/ui/badge'
import { Search, Wifi } from 'lucide-react'

interface GooseMessage {
  appId: string
  goCBRef: string
  macSrc: string
  lastSeen: string
}

export default function GoosePage() {
  const [isScanning, setIsScanning] = useState(false)
  const [messages, setMessages] = useState<GooseMessage[]>([])
  const [tripRule, setTripRule] = useState('RelayA_Trip/LLN0.Ind1.stVal == true')

  const handleScan = () => {
    setIsScanning(true)
    setTimeout(() => {
      setMessages([
        { appId: '0x0001', goCBRef: 'RelayA/LLN0$GO$Trip', macSrc: '01:0C:CD:01:00:01', lastSeen: '2s ago' },
        { appId: '0x0002', goCBRef: 'RelayB/LLN0$GO$Status', macSrc: '01:0C:CD:01:00:02', lastSeen: '1s ago' },
      ])
      setIsScanning(false)
    }, 1500)
  }

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-3xl font-bold tracking-tight">GOOSE Monitor</h1>
        <p className="text-muted-foreground">Monitor and subscribe to IEC 61850 GOOSE messages</p>
      </div>

      <div className="grid gap-6 lg:grid-cols-2">
        <Card>
          <CardHeader>
            <div className="flex items-center justify-between">
              <div><CardTitle>Message Discovery</CardTitle><CardDescription>Scan network for GOOSE messages</CardDescription></div>
              <Button onClick={handleScan} disabled={isScanning} size="sm">
                {isScanning ? <Wifi className="mr-2 h-4 w-4 animate-pulse" /> : <Search className="mr-2 h-4 w-4" />}
                {isScanning ? 'Scanning...' : 'Scan'}
              </Button>
            </div>
          </CardHeader>
          <CardContent>
            {messages.length === 0 ? (
              <p className="text-sm text-muted-foreground text-center py-4">No messages found. Click Scan to discover GOOSE traffic.</p>
            ) : (
              <div className="space-y-2">
                {messages.map((msg, idx) => (
                  <div key={idx} className="p-3 border rounded-lg">
                    <div className="flex items-center justify-between mb-2">
                      <span className="font-mono text-sm">{msg.goCBRef}</span>
                      <Badge variant="outline">{msg.lastSeen}</Badge>
                    </div>
                    <div className="text-xs text-muted-foreground space-y-1">
                      <p>App ID: {msg.appId}</p>
                      <p>MAC: {msg.macSrc}</p>
                    </div>
                  </div>
                ))}
              </div>
            )}
          </CardContent>
        </Card>

        <Card>
          <CardHeader><CardTitle>Trip Rule Configuration</CardTitle><CardDescription>Define trip condition expression</CardDescription></CardHeader>
          <CardContent className="space-y-4">
            <div className="space-y-2">
              <Label htmlFor="trip-rule">Trip Rule Expression</Label>
              <Input id="trip-rule" value={tripRule} onChange={(e) => setTripRule(e.target.value)} placeholder="e.g., RelayA_Trip/LLN0.Ind1.stVal == true" className="font-mono text-sm" />
            </div>
            <div className="p-3 bg-muted rounded-lg text-sm space-y-1">
              <p className="font-medium">Examples:</p>
              <p className="text-xs text-muted-foreground">• RelayA_Trip/LLN0.Ind1.stVal == true</p>
              <p className="text-xs text-muted-foreground">• Breaker/XCBR1.Pos.stVal == 0</p>
            </div>
            <Button className="w-full">Apply Trip Rule</Button>
          </CardContent>
        </Card>
      </div>
    </div>
  )
}
